#include <stdlib.h>
#include <errno.h>
#include <ifaddrs.h>
#include <fcntl.h>
#include <math.h>
#include <net/if.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h>
#include <inttypes.h>
#include <netdb.h>

#include "log_uploader.h"
#include "kvextract.h"
#include "upload.h"
#include "utils.h"

#define DEFAULT_SERVER "https://diag.cpe.gfsvc.com"
#define COUNTER_MARKER_FILE "/tmp/loguploadcounter"
#define LOGS_UPLOADED_MARKER_FILE "/tmp/logs-uploaded"
#define DEFAULT_UPLOAD_TARGET "dmesg"
#define MAX_LOG_SIZE 8*1024*1024 // 8 MB
#define DEV_KMSG_PATH "/dev/kmsg"
#define NTP_SYNCED_PATH "/tmp/ntp.synced"
#define VERSION_PATH "/etc/version"
#define SERIAL_PATH "/etc/serial"
#define PLATFORM_PATH "/etc/platform"
// 8192 is the size of the buffer used in printk.c to store a line read from
// /dev/kmsg before copying it into our userspace buffer
#define LOG_LINE_BUFFER_SIZE 8192

// Use level 1 so we compress for speed, not size since we have lots
// of bandwidth for uploading but CPU cycles are something we want to
// conserve.
#define ZLIB_COMPRESS_LEVEL 1

static const char *interfaces_to_check[] = { "br0", "eth0", "man", "pon0" };
static int num_interfaces = sizeof(interfaces_to_check) /
  sizeof(interfaces_to_check[0]);

// To allow overriding for testing.
int getnameinfo_resolver(const struct sockaddr* sa, socklen_t salen, char* host,
    size_t hostlen, char* serv, size_t servlen, int flags) {
  return getnameinfo(sa, salen, host, hostlen, serv, servlen, flags);
}

// To allow overriding for testing, gets the MAC of a network interface.
int iface_to_mac(const char* iface, char* buf, int len) {
  // xx:xx:xx:xx:xx:xx format comes back, 17 chars + terminator
  if (len < 18)
    return -1;
  struct ifreq ifreq;
  int fd;
  memset(&ifreq, 0, sizeof(ifreq));
  fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
  if (strlen(iface) + 1 > sizeof(ifreq.ifr_name))
    return -1;
  strcpy(ifreq.ifr_name, iface);
  if (ioctl(fd, SIOCGIFHWADDR, &ifreq) == -1) {
    close(fd);
    return -1;
  } else {
    close(fd);
    snprintf(buf, len, "%02x:%02x:%02x:%02x:%02x:%02x",
        ifreq.ifr_hwaddr.sa_data[0], ifreq.ifr_hwaddr.sa_data[1],
        ifreq.ifr_hwaddr.sa_data[2], ifreq.ifr_hwaddr.sa_data[3],
        ifreq.ifr_hwaddr.sa_data[4], ifreq.ifr_hwaddr.sa_data[5]);
    return 0;
  }
}

int standard_read(char* buffer, int len, void* user_data) {
  return read(*((int*) user_data), buffer, len);
}

static void usage(const char* progname) {
  fprintf(stderr, "Usage for: %s\n", progname);
  fprintf(stderr, " --server URL Server URL (default: " DEFAULT_SERVER ")\n");
  fprintf(stderr, " --all        Upload entire logs, not just new data\n");
  fprintf(stderr, " --stdout     Print to stdout instead of uploading\n");
  fprintf(stderr, " --stdin name Get data from stdin rather than /dev/kmsg and"
      " upload to 'name' facility rather than 'dmesg', also disables "
      "looping\n");
  exit(EXIT_SUCCESS);
}

static int parse_args(struct upload_config* config, int argc,
    char* const argv[]) {
  int opt = 0;
  static struct option long_options[] = {
    { "server", required_argument, 0, 's' },
    { "all", no_argument, 0, 'a' },
    { "stdout", no_argument, 0, 'd' },
    { "stdin", required_argument, 0, 'i' },
    { "logtype", required_argument, 0 , 'l' },
    { 0, 0, 0, 0}
  };

  while (1) {
    opt = getopt_long(argc, argv, "", long_options, NULL);
    if (opt == -1)
      break;

    switch (opt) {
      case 'a':
        config->upload_all = 1;
        break;
      case 'd':
        config->use_stdout = 1;
        break;
      case 's':
        snprintf(config->server, sizeof(config->server), "%s", optarg);
        break;
      case 'i':
        config->use_stdin = 1;
        snprintf(config->upload_target, sizeof(config->upload_target), "%s",
            optarg);
        break;
      case 'l':
        snprintf(config->logtype, sizeof(config->logtype), "%s", optarg);
        break;
     default:
       return -1;
    }
  }
  if (optind < argc)
    return -1; // extraneous non-option arguments
  return 0;
}

int main(int argc, char* const argv[]) {
  setvbuf(stdout, (char *) NULL, _IOLBF, 0);

  struct upload_config config;
  memset(&config, 0, sizeof(config));

  snprintf(config.server, sizeof(config.server), "%s", DEFAULT_SERVER);
  snprintf(config.upload_target, sizeof(config.upload_target), "%s",
        DEFAULT_UPLOAD_TARGET);

  if (argc > 1) {
    if (parse_args(&config, argc, argv) < 0) {
      usage(argv[0]);
      return 1;
    }
  }

  // Initiliaze the random number generator
  srandom(getpid() ^ time(NULL));

  // Allocate this once and re-use it very time
  char* log_buffer = (char*) malloc(MAX_LOG_SIZE);
  if (!log_buffer) {
    fprintf(stderr, "Failed to allocate log_buffer!\n");
    return 1;
  }

  int kmsg_read_fd = 0;
  if (!config.use_stdin) {
    // We never actually close this, it'll get done when the process exits.
    // Use nonblocking mode so we know when we're consumed all the recent data.
    kmsg_read_fd = open("/dev/kmsg", O_RDONLY | O_NONBLOCK);
    if (kmsg_read_fd < 0) {
      fprintf(stderr, "failed to open /dev/kmsg for reading: %s\n",
          strerror(errno));
      return 1;
    }
  }

  // In the kernel struct devkmsg_user this is the size of the buffer
  // they use to read a line and then copy it to us, so it won't be any
  // bigger than this.
  char line_buffer[LOG_LINE_BUFFER_SIZE];
  unsigned long total_read;
  z_stream zstrm;
  memset(&zstrm, 0, sizeof(zstrm));

  struct log_parse_params parse_params;
  memset(&parse_params, 0, sizeof(parse_params));
  parse_params.config = &config;
  parse_params.user_data = &kmsg_read_fd;
  parse_params.read_log_data = standard_read;
  parse_params.dev_kmsg_path = DEV_KMSG_PATH;
  parse_params.version_path = VERSION_PATH;
  parse_params.ntp_synced_path = NTP_SYNCED_PATH;
  // This'll set it to zero if it can't read the file which is fine.
  parse_params.last_log_counter = read_file_as_uint64(COUNTER_MARKER_FILE);
  parse_params.log_buffer = log_buffer;
  parse_params.line_buffer = line_buffer;
  parse_params.line_buffer_size = sizeof(line_buffer);

  while (1) {
    char* log_data_to_use;
    if (config.use_stdin) {
      // Read in all the data from stdin
      int num_read;
      total_read = num_read = 0;
      while ((num_read = read(STDIN_FILENO, log_buffer + total_read,
              MAX_LOG_SIZE - total_read)) > 0) {
        total_read += num_read;
      }
      if (num_read < 0) {
        fprintf(stderr, "failed reading from stdin: %s\n", strerror(errno));
        return 1;
      }
      log_buffer[total_read] = '\0';
      log_data_to_use = log_buffer;
    } else {
      // Remove the marker file to indicate we've completed the upload process.
      remove(LOGS_UPLOADED_MARKER_FILE);

      // Normal logs processing, write out the general marker data.
      // We only need to write this out if we're doing the first upload
      // otherwise it'll have been written right after we did the last one.
      if (parse_params.last_log_counter == 0 &&
          logmark_once(DEV_KMSG_PATH, VERSION_PATH,
          NTP_SYNCED_PATH)) {
        fprintf(stderr, "failed to execute logmark-once properly\n");
        return 1;
      }

      parse_params.total_read = MAX_LOG_SIZE;
      log_data_to_use = parse_and_consume_log_data(&parse_params);
      if (!log_data_to_use) {
        fprintf(stderr, "failed with logs parsing, abort!\n");
        return 1;
      }
      total_read = parse_params.total_read;
    }

    // Now we've read all of the log data into our buffer, proceed
    // with uploading or outputting it.

    if (config.use_stdout) {
      // Just print the whole thing to stdout
      printf("%s", log_data_to_use);
    } else {
      struct ifaddrs* ifaddr;
      if (getifaddrs(&ifaddr)) {
        fprintf(stderr, "failed calling getifaddrs: %s\n", strerror(errno));
        return 1;
      }

      struct kvextractparams kvparams;
      memset(&kvparams, 0, sizeof(kvparams));
      kvparams.interfaces_to_check = interfaces_to_check;
      kvparams.num_interfaces = num_interfaces;
      kvparams.ifaddr = ifaddr;
      kvparams.platform_path = PLATFORM_PATH;
      kvparams.serial_path = SERIAL_PATH;
      kvparams.name_info_resolver = getnameinfo_resolver;
      kvparams.interface_to_mac = iface_to_mac;
      kvparams.logtype = config.logtype;
      struct kvpair* kvpairs = extract_kv_pairs(&kvparams);
      freeifaddrs(ifaddr);
      if (!kvpairs) {
        fprintf(stderr, "failure extracting kv pairs, abort\n");
        return 1;
      }

      // Adjust this if we moved the pointer.
      unsigned long compressed_size = MAX_LOG_SIZE -
        (log_data_to_use - log_buffer);
      int comp_result = deflate_inplace(&zstrm, (unsigned char*)log_data_to_use,
          total_read, &compressed_size);
      if (comp_result != Z_OK) {
        return 1;
      }

      int upload_res = upload_file(config.server, config.upload_target,
            log_data_to_use, compressed_size, kvpairs);
      free_kv_pairs(kvpairs);
      if (upload_res) {
        return 1;
      }
      if (write_file_as_uint64(COUNTER_MARKER_FILE,
            parse_params.last_log_counter)) {
        fprintf(stderr, "unable to write out last log counter\n");
        return 1;
      }
      // Write the marker file to indicate we finished the upload.
      int marker_fd = open(LOGS_UPLOADED_MARKER_FILE, O_CREAT | O_WRONLY,
          RW_FILE_PERMISSIONS);
      if (marker_fd >= 0)
        close(marker_fd);
    }

    if (write_to_file(DEV_KMSG_PATH, LOG_MARKER_END_LINE) < 0) {
      fprintf(stderr, "failed to write out end marker\n");
      return 1;
    }

    if (!config.use_stdin) {
      // Randomize the sleep time between 55 and 65 seconds which matches
      // what is done in the log-delay script.
      int sleep_dur = 55 + (random() % 11);
      sleep(sleep_dur);
    } else
      break;
  }
  free(log_buffer);
  return 0;
}
