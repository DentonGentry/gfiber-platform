// Copyright 2011 Google Inc. All Rights Reserved.
// Author: dgentry@google.com (Denny Gentry)

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hmx_upgrade_nvram.h"

// Max length of data in an NVRAM field
#define NVRAM_MAX_DATA  (64*1024)

// Number of bytes of GPN to be represented as hex data
#define GPN_HEX_BYTES 4

// Holds whether -w can create new variables in NVRAM. Set with -n
int can_add_flag = 0;

/* To avoid modifying the HMX code, we supply dummy versions of two
 * missing routines to satisfy the linker. These are used when writing
 * the complete NVRAM partiton, which we do not need in this utility. */
DRV_Error DRV_NANDFLASH_GetNvramHandle(int handle) {
  return DRV_ERR;
}
DRV_Error DRV_FLASH_Write(int offset, char* data, int nDataSize) {
  return DRV_ERR;
}

void usage(const char* progname) {
  printf("Usage: %s [-d | [-q|-b] [-r|-k] VARNAME] [ [-n [-p [RO|RW]]] -w VARNAME=value]\n", progname);
  printf("\t-d : dump all NVRAM variables\n");
  printf("\t-r VARNAME : read VARNAME from NVRAM\n");
  printf("\t-q : quiet mode, suppress the variable name and equal sign\n");
  printf("\t-b : read VARNAME from NVRAM in raw binary format, e.g. dumping a binary key\n");
  printf("\t-w VARNAME=value : write value to VARNAME in NVRAM.\n");
  printf("\t-n : toggles whether -w can create new variables. Default is off\n");
  printf("\t-p [RW|RO] : toggles what partition new writes (-n) used. Default is RW\n");
  printf("\t-k VARNAME : delete existing key/value pair from NVRAM.\n");
  printf("\t Set environment variable: $HNVRAM_LOCATION to change where read/writes are performed.");
  printf("\t By default hnvram uses '/dev/mtd/hnvram'\n");
}

// Format of data in the NVRAM
typedef enum {
  HNVRAM_STRING,    // NUL-terminated string
  HNVRAM_MAC,       // 00:11:22:33:44:55
  HNVRAM_HMXSWVERS, // 2.15
  HNVRAM_UINT8,     // a single byte, generally 0/1 for a boolean.
  HNVRAM_GPN,       // Two formats:
                    // - 4 bytes (old format): printed as 8 digit hex.
                    // - > 4 bytes (new format): printed as NULL-terminated
                    //  string.
  HNVRAM_HEXSTRING  // hexbinary
} hnvram_format_e;

typedef struct hnvram_field_s {
  const char* name;
  NVRAM_FIELD_T nvram_type;  // defined in hmx_upgrade_nvram.h
  hnvram_format_e format;
} hnvram_field_t;

const hnvram_field_t nvram_fields[] = {
  {"SYSTEM_ID",            NVRAM_FIELD_SYSTEM_ID,            HNVRAM_STRING},
  {"MAC_ADDR",             NVRAM_FIELD_MAC_ADDR,             HNVRAM_MAC},
  {"SERIAL_NO",            NVRAM_FIELD_SERIAL_NO,            HNVRAM_STRING},
  {"LOADER_VERSION",       NVRAM_FIELD_LOADER_VERSION,       HNVRAM_HMXSWVERS},
  {"ACTIVATED_KERNEL_NUM", NVRAM_FIELD_ACTIVATED_KERNEL_NUM, HNVRAM_UINT8},
  {"MTD_TYPE_FOR_KERNEL",  NVRAM_FIELD_MTD_TYPE_FOR_KERNEL,  HNVRAM_STRING},
  {"ACTIVATED_KERNEL_NAME", NVRAM_FIELD_ACTIVATED_KERNEL_NAME, HNVRAM_STRING},
  {"EXTRA_KERNEL_OPT",     NVRAM_FIELD_EXTRA_KERNEL_OPT,     HNVRAM_STRING},
  {"PLATFORM_NAME",        NVRAM_FIELD_PLATFORM_NAME,     HNVRAM_STRING},
  {"1ST_SERIAL_NUMBER",    NVRAM_FIELD_1ST_SERIAL_NUMBER, HNVRAM_STRING},
  {"2ND_SERIAL_NUMBER",    NVRAM_FIELD_2ND_SERIAL_NUMBER, HNVRAM_STRING},
  {"GPN",                  NVRAM_FIELD_GPN,               HNVRAM_GPN},
  {"MAC_ADDR_MOCA",        NVRAM_FIELD_MAC_ADDR_MOCA,     HNVRAM_MAC},
  {"MAC_ADDR_BT",          NVRAM_FIELD_MAC_ADDR_BT,       HNVRAM_MAC},
  {"MAC_ADDR_WIFI",        NVRAM_FIELD_MAC_ADDR_WIFI,     HNVRAM_MAC},
  {"MAC_ADDR_WIFI2",       NVRAM_FIELD_MAC_ADDR_WIFI2,    HNVRAM_MAC},
  {"MAC_ADDR_WAN",         NVRAM_FIELD_MAC_ADDR_WAN,      HNVRAM_MAC},
  {"HDCP_KEY",             NVRAM_FIELD_HDCP_KEY,          HNVRAM_HEXSTRING},
  {"DTCP_KEY",             NVRAM_FIELD_DTCP_KEY,          HNVRAM_HEXSTRING},
  {"GOOGLE_SSL_PEM",       NVRAM_FIELD_GOOGLE_SSL_PEM,    HNVRAM_STRING},
  {"GOOGLE_SSL_CRT",       NVRAM_FIELD_GOOGLE_SSL_CRT,    HNVRAM_STRING},
  {"PAIRED_DISK",          NVRAM_FIELD_PAIRED_DISK,       HNVRAM_STRING},
  {"PARTITION_VER",        NVRAM_FIELD_PARTITION_VER,     HNVRAM_STRING},
  {"HW_VER",               NVRAM_FIELD_HW_VER,            HNVRAM_UINT8},
  {"UITYPE",               NVRAM_FIELD_UITYPE,            HNVRAM_STRING},
  {"LASER_CHANNEL",        NVRAM_FIELD_LASER_CHANNEL,     HNVRAM_STRING},
  {"MAC_ADDR_PON",         NVRAM_FIELD_MAC_ADDR_PON,      HNVRAM_MAC},
  {"PRODUCTION_UNIT",      NVRAM_FIELD_PRODUCTION_UNIT,   HNVRAM_STRING},
  {"BOOT_TARGET",          NVRAM_FIELD_BOOT_TARGET,       HNVRAM_STRING},
  {"ANDROID_ACTIVE_PARTITION", NVRAM_FIELD_ANDROID_ACTIVE_PARTITION,
   HNVRAM_STRING},
};

const hnvram_field_t* get_nvram_field(const char* name) {
  int nentries = sizeof(nvram_fields) / sizeof(nvram_fields[0]);
  int i;

  for (i = 0; i < nentries; ++i) {
    const hnvram_field_t* map = &nvram_fields[i];
    if (strcasecmp(name, map->name) == 0) {
      return map;
    }
  }

  return NULL;
}


// ------------------ READ NVRAM -----------------------------


void format_string(const unsigned char* data, char* output, int outlen) {
  snprintf(output, outlen, "%s", data);
}

void format_mac(const unsigned char* data, char* output, int outlen) {
  snprintf(output, outlen, "%02hx:%02hx:%02hx:%02hx:%02hx:%02hx",
           data[0], data[1], data[2], data[3], data[4], data[5]);
}

void format_hmxswvers(const unsigned char* data, char* output, int outlen) {
  snprintf(output, outlen, "%hhu.%hhu", data[1], data[0]);
}

void format_uint8(const unsigned char* data, char* output, int outlen) {
  snprintf(output, outlen, "%u", data[0]);
}

void format_hexstring(const unsigned char* data, int datalen, char* output,
                      int outlen) {
  int i;
  if (outlen < (datalen * 2 + 1)) {
    fprintf(stderr, "%s buffer too small %d < %d",
            __FUNCTION__, outlen, (datalen * 2 + 1));
    exit(1);
  }
  for (i = 0; i < datalen; ++i) {
    snprintf(output + (i * 2), 3, "%02x", data[i]);
  }
}

void format_gpn(const unsigned char* data, const int data_len, char* output,
                int outlen) {
  // Format first 4 bytes as 8 digit hex.
  if (data_len == GPN_HEX_BYTES)
    format_hexstring(data, GPN_HEX_BYTES, output, outlen);
  else
    format_string(data, output, outlen);
}

char* format_nvram(hnvram_format_e format, const unsigned char* data,
                   const int data_len, char* output, int outlen) {
  output[0] = '\0';
  switch(format) {
    case HNVRAM_STRING:    format_string(data, output, outlen); break;
    case HNVRAM_MAC:       format_mac(data, output, outlen); break;
    case HNVRAM_HMXSWVERS: format_hmxswvers(data, output, outlen); break;
    case HNVRAM_UINT8:     format_uint8(data, output, outlen); break;
    case HNVRAM_GPN:       format_gpn(data, data_len, output, outlen); break;
    case HNVRAM_HEXSTRING: format_hexstring(data, data_len, output, outlen);
                           break;
  }
  return output;
}

int read_raw_nvram(const char* name, char* output, int outlen) {
  const hnvram_field_t* field = get_nvram_field(name);
  unsigned int ret;
  if (field == NULL) {
    return -1;
  }

  if (HMX_NVRAM_GetLength(field->nvram_type, &ret) != DRV_OK) {
    return -1;
  }

  if (ret > outlen) {
    return -1;
  }

  if (HMX_NVRAM_GetField(field->nvram_type, 0, output, outlen) != DRV_OK) {
    return -1;
  }

  return (int)ret;
}

// name - name of key to be read
// output - buffer for value of key
// outlen - length of buffer
// quiet - whether buffer is KEY=VAL or VAL
// part_used - in the case of dynamically added variables (is_field = false),
//     returns what partition we found the key in
char* read_nvram(const char* name, char* output, int outlen, int quiet,
                 HMX_NVRAM_PARTITION_E* part_used) {
  const hnvram_field_t* field = get_nvram_field(name);
  int is_field = (field != NULL);

  unsigned char data[NVRAM_MAX_DATA] = {0};
  unsigned int data_len = 0;
  hnvram_format_e format_type;
  if (is_field) {
    format_type = field->format;
    if (HMX_NVRAM_GetField(field->nvram_type, 0, data, sizeof(data)) != DRV_OK ||
        HMX_NVRAM_GetLength(field->nvram_type, &data_len) != DRV_OK) {
      return NULL;
    }
  } else {
    format_type = HNVRAM_STRING;

    // Try both partitions
    *part_used = HMX_NVRAM_PARTITION_RW;
    DRV_Error e = HMX_NVRAM_Read(*part_used, (unsigned char*)name, 0, data,
                                 sizeof(data), &data_len);
    if (e != DRV_OK) {
      *part_used = HMX_NVRAM_PARTITION_RO;
      e = HMX_NVRAM_Read(*part_used, (unsigned char*)name, 0, data,
                         sizeof(data), &data_len);
      if (e != DRV_OK) {
        return NULL;
      }
    }
  }
  char formatbuf[NVRAM_MAX_DATA * 2];
  char* nv = format_nvram(format_type, data, data_len, formatbuf,
                          sizeof(formatbuf));
  if (quiet) {
    snprintf(output, outlen, "%s", nv);
  } else {
    snprintf(output, outlen, "%s=%s", name, nv);
  }
  return output;
}
// ----------------- WRITE NVRAM -----------------------------


unsigned char* parse_string(const char* input,
                            unsigned char* output, unsigned int* outlen) {
  int len = strlen(input);
  if (len > *outlen) {
    // Data is too large, don't permit a partial write.
    return NULL;
  }

  strncpy((char*)output, input, len);
  *outlen = len;
  return output;
}

unsigned char* parse_mac(const char* input,
                         unsigned char* output, unsigned int* outlen) {
  if (*outlen < 6) return NULL;

  if (sscanf(input, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
             &output[0], &output[1], &output[2],
             &output[3], &output[4], &output[5]) != 6) {
    return NULL;
  }
  *outlen = 6;
  return output;
}

unsigned char* parse_hmxswvers(const char* input,
                               unsigned char* output, unsigned int* outlen) {
  if (*outlen < 2) return NULL;

  if (sscanf(input, "%hhd.%hhd", &output[1], &output[0]) != 2) {
    return NULL;
  }
  *outlen = 2;
  return output;
}

unsigned char* parse_uint8(const char* input,
                           unsigned char* output, unsigned int* outlen) {
  if (*outlen < 1) return NULL;

  output[0] = input[0] - '0';
  *outlen = 1;
  return output;
}

int parse_hexdigit(unsigned char c) {
  switch(c) {
    case '0' ... '9': return c - '0';
    case 'a' ... 'f': return 10 + (c - 'a');
    case 'A' ... 'F': return 10 + (c - 'A');
    default: return 0xff;
  }
}

unsigned char* parse_hexstring(const char* input,
                               unsigned char* output, unsigned int* outlen) {
  unsigned int i, len = strlen(input) / 2;
  if (*outlen < len) {
    len = *outlen;
  }

  for (i = 0; i < len; ++i) {
    unsigned char c;
    output[i] = parse_hexdigit(input[2*i]) << 4 |
                parse_hexdigit(input[2*i+1]);
  }

  *outlen = len;
  return output;
}

int is_hexstring(const char* input, int hex_len) {
  int i = 0;
  for (i = 0; i < hex_len; i++) {
    if (!isxdigit(input[i])) {
      return 0;
    }
  }
  if (input[hex_len] != '\0') {
    return 0;
  }
  return 1;
}

unsigned char* parse_gpn(const char* input,
                         unsigned char* output, unsigned int* outlen) {
  if (*outlen < 4) return NULL;

  // Old GPN format: 8-digit hex string
  if (is_hexstring(input, GPN_HEX_BYTES * 2)) {
    if (sscanf(input, "%02hhx%02hhx%02hhx%02hhx",
               &output[0], &output[1], &output[2], &output[3]) != GPN_HEX_BYTES) {
      return NULL;
    }
    *outlen = GPN_HEX_BYTES;
    return output;
  }

  // New GPN format: regular string
  return parse_string(input, output, outlen);
}

unsigned char* parse_nvram(hnvram_format_e format, const char* input,
                           unsigned char* output, unsigned int* outlen) {
  output[0] = '\0';
  switch(format) {
    case HNVRAM_STRING:
      return parse_string(input, output, outlen);
      break;
    case HNVRAM_MAC:
      return parse_mac(input, output, outlen);
      break;
    case HNVRAM_HMXSWVERS:
      return parse_hmxswvers(input, output, outlen);
      break;
    case HNVRAM_UINT8:
      return parse_uint8(input, output, outlen);
      break;
    case HNVRAM_GPN:
      return parse_gpn(input, output, outlen);
      break;
    case HNVRAM_HEXSTRING:
      return parse_hexstring(input, output, outlen);
      break;
  }
  return NULL;
}

DRV_Error clear_nvram(const char* optarg) {
  DRV_Error err1 = HMX_NVRAM_Remove(HMX_NVRAM_PARTITION_RW,
                                    (unsigned char*)optarg);
  DRV_Error err2 = HMX_NVRAM_Remove(HMX_NVRAM_PARTITION_RO,
                                    (unsigned char*)optarg);

  // Avoid throwing error message if variable already cleared
  if ((err1 == DRV_ERR || err1 == DRV_OK) &&
      (err2 == DRV_ERR || err2 == DRV_OK)) {
    return DRV_OK;
  }

  fprintf(stderr, "Error while deleting key %s. RW: %d RO: %d.\n", optarg,
          err1, err2);
  return DRV_ERR;
}


int write_nvram(const char* name, const char* value,
                HMX_NVRAM_PARTITION_E desired_part) {
  const hnvram_field_t* field = get_nvram_field(name);
  int is_field = (field != NULL);

  hnvram_format_e format_type;
  if (is_field) {
    format_type = field->format;
  } else {
    format_type = HNVRAM_STRING;
  }

  if (strlen(value) > NVRAM_MAX_DATA) {
    fprintf(stderr, "Value length %d exceeds maximum data size of %d\n",
      (int)strlen(value), NVRAM_MAX_DATA);
    return -1;
  }

  unsigned char nvram_value[NVRAM_MAX_DATA];
  unsigned int nvram_len = sizeof(nvram_value);
  if (parse_nvram(format_type, value, nvram_value, &nvram_len) == NULL) {
    return -2;
  }

  if (!is_field) {
    char tmp[NVRAM_MAX_DATA] = {0};
    HMX_NVRAM_PARTITION_E part_used;
    if (read_nvram(name, tmp, NVRAM_MAX_DATA, 1, &part_used) == NULL) {
      return -3; // Write failed: Variable not found
    }

    if (desired_part != HMX_NVRAM_PARTITION_UNSPECIFIED &&
        desired_part != part_used) {
      fprintf(stderr, "Variable already exists in other partition: %s\n", name);
      return -4;
    }

    DRV_Error er = HMX_NVRAM_Write(part_used, (unsigned char*)name, 0,
                                   nvram_value, nvram_len);
    if (er != DRV_OK) {
      return -5;
    }
  } else {
    if (desired_part != HMX_NVRAM_PARTITION_UNSPECIFIED) {
      fprintf(stderr, "Partition was specified (%d) on a field variable: %s\n",
              desired_part, name);
      return -6;
    }
    if (HMX_NVRAM_SetField(field->nvram_type, 0,
                           nvram_value, nvram_len) != DRV_OK) {
      return -7;
    }
  }

  return 0;
}

// Adds new variable to HNVRAM in desired_partition as STRING
int write_nvram_new(const char* name, const char* value,
                    HMX_NVRAM_PARTITION_E desired_part) {
  if (!can_add_flag) {
    fprintf(stderr, "Key not found in NVRAM. Add -n to allow creation %s\n",
            name);
    return -1;
  }

  char tmp[NVRAM_MAX_DATA] = {0};
  unsigned char nvram_value[NVRAM_MAX_DATA];
  unsigned int nvram_len = sizeof(nvram_value);
  if (parse_nvram(HNVRAM_STRING, value, nvram_value, &nvram_len) == NULL) {
    return -2;
  }

  if (desired_part == HMX_NVRAM_PARTITION_UNSPECIFIED) {
    desired_part = HMX_NVRAM_PARTITION_RW;
  }

  DRV_Error er = HMX_NVRAM_Write(desired_part, (unsigned char*)name, 0,
                                 nvram_value, nvram_len);
  if (er != DRV_OK) {
    return -3;
  }

  return 0;
}

int init_nvram() {
  const char* location = secure_getenv("HNVRAM_LOCATION");
  return (int)HMX_NVRAM_Init(location);
}

int hnvram_main(int argc, char* const argv[]) {
  DRV_Error err;

  libupgrade_verbose = 0;

  int ret = init_nvram();
  if (ret != 0) {
    fprintf(stderr, "NVRAM Init failed: %d\n", ret);
    exit(1);
  }

  char op = 0;     // operation
  int op_cnt = 0;  // operation
  int q_flag = 0;  // quiet: don't output name of variable.
  int b_flag = 0;  // binary: output the binary format
  // Desired partition for new writes.
  HMX_NVRAM_PARTITION_E desired_part = HMX_NVRAM_PARTITION_UNSPECIFIED;
  char output[NVRAM_MAX_DATA];
  int c;
  while ((c = getopt(argc, argv, "dbqrnp:w:k:")) != -1) {
    switch(c) {
      case 'b':
        b_flag = 1;
        break;
      case 'q':
        q_flag = 1;
        break;
      case 'n':
        can_add_flag = 1;
        break;
      case 'p':
        if (strcmp(optarg, "RO") == 0) {
          desired_part = HMX_NVRAM_PARTITION_RO;
        } else if (strcmp(optarg, "RW") == 0) {
          desired_part = HMX_NVRAM_PARTITION_RW;
        } else {
          fprintf(stderr, "Invalid partition: %s. Use RW or RO\n", optarg);
          exit(1);
        }
        break;
      case 'w':
        {
          char* duparg = strdup(optarg);
          char* equal = strchr(duparg, '=');
          if (equal == NULL) {
            return -1;
          }

          char* name = duparg;
          *equal = '\0';
          char* value = equal + 1;

          int ret = write_nvram(name, value, desired_part);
          if (ret == -3) {
            // key not found, try to add a new one
            ret = write_nvram_new(name, value, desired_part);
          }

          if (ret != 0) {
            fprintf(stderr, "Err %d: Unable to write %s\n", ret, duparg);
            free(duparg);
            exit(1);
          }
          free(duparg);
        }
        break;
      case 'k':
        {
          char* duparg = strdup(optarg);
          if (clear_nvram(duparg) != DRV_OK) {
            fprintf(stderr, "Unable to remove key %s\n", duparg);
            free(duparg);
            exit(1);
          }
          free(duparg);
        }
        break;
      case 'r':
      case 'd':
        if (op != c) {
          ++op_cnt;
        }
        op = c;
        break;
      default:
        usage(argv[0]);
        exit(1);
    }
  }

  if (op_cnt > 1) {
    usage(argv[0]);
    exit(1);
  }

  // dump NVRAM at the end, after all writes have been done.
  switch (op) {
    case 'd':
      if (optind < argc) {
        usage(argv[0]);
        exit(1);
      }
      if ((err = HMX_NVRAM_Dir()) != DRV_OK) {
        fprintf(stderr, "Unable to dump variables, HMX_NVRAM_Dir=%d\n", err);
      }
      break;
    case 'r':
      if (optind >= argc) {
        usage(argv[0]);
        exit(1);
      }
      for (; optind < argc; ++optind) {
        if (b_flag) {
          int len = read_raw_nvram(argv[optind], output, sizeof(output));
          if (len < 0) {
            fprintf(stderr, "Unable to read %s\n", argv[optind]);
            exit(1);
          }
          fwrite(output, 1, len, stdout);
        } else {
          HMX_NVRAM_PARTITION_E part_used;
          if (read_nvram(argv[optind], output, sizeof(output), q_flag, &part_used) == NULL) {
            fprintf(stderr, "Unable to read %s\n", argv[optind]);
            exit(1);
          }
          puts(output);
        }
      }
      break;
  }

  exit(0);
}

#ifndef TEST_MAIN
int main(int argc, char* const argv[]) {
  return hnvram_main(argc, argv);
}
#endif  // TEST_MAIN
