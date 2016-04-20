// Copyright 2011 Google Inc. All Rights Reserved.
// Author: dgentry@google.com (Denny Gentry)

#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hmx_upgrade_nvram.h"

// Max length of data in an NVRAM field
#define NVRAM_MAX_DATA  4096

// Number of bytes of GPN to be represented as hex data
#define GPN_HEX_BYTES 4

/* To avoid modifying the HMX code, we supply dummy versions of two
 * missing routines to satisfy the linker. These are used when writing
 * the complete NVRAM partiton, which we do not need in this utility. */
DRV_Error DRV_NANDFLASH_GetNvramHandle(int handle) {
  return DRV_ERR;
}
DRV_Error DRV_FLASH_Write(int offset, char *data, int nDataSize) {
  return DRV_ERR;
}

void usage(const char* progname) {
  printf("Usage: %s [-d | [-q|-b] -r VARNAME] [-w VARNAME=value]\n", progname);
  printf("\t-d : dump all NVRAM variables\n");
  printf("\t-r VARNAME : read VARNAME from NVRAM\n");
  printf("\t-q : quiet mode, suppress the variable name and equal sign\n");
  printf("\t-b : read VARNAME from NVRAM in raw binary format, e.g. dumping a binary key\n");
  printf("\t-w VARNAME=value : write value to VARNAME in NVRAM.\n");
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


void format_string(const char* data, char* output, int outlen) {
  snprintf(output, outlen, "%s", data);
}

void format_mac(const char* data, char* output, int outlen) {
  const unsigned char* mac = (const unsigned char*) data;
  snprintf(output, outlen, "%02hx:%02hx:%02hx:%02hx:%02hx:%02hx",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void format_hmxswvers(const char* data, char* output, int outlen) {
  const unsigned char* udata = (const unsigned char*) data;
  snprintf(output, outlen, "%hhu.%hhu", udata[1], udata[0]);
}

void format_uint8(const char* data, char* output, int outlen) {
  const unsigned char* d = (const unsigned char*)data;
  snprintf(output, outlen, "%u", d[0]);
}

void format_hexstring(const char* data, int datalen, char* output, int outlen) {
  const unsigned char* d = (const unsigned char*)data;
  int i;
  if (outlen < (datalen * 2 + 1)) {
    fprintf(stderr, "%s buffer too small %d < %d",
            __FUNCTION__, outlen, (datalen * 2 + 1));
    exit(1);
  }
  for (i = 0; i < datalen; ++i) {
    snprintf(output + (i * 2), 3, "%02x", d[i]);
  }
}

void format_gpn(const char* data, const int data_len, char* output,
                int outlen) {
  // Format first 4 bytes as 8 digit hex.
  if (data_len == GPN_HEX_BYTES)
    format_hexstring(data, GPN_HEX_BYTES, output, outlen);
  else
    format_string(data, output, outlen);
}

char* format_nvram(hnvram_format_e format, const char* data,
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
  int ret;
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

  return ret;
}

char* read_nvram(const char* name, char* output, int outlen, int quiet) {
  const hnvram_field_t* field = get_nvram_field(name);
  if (field == NULL) {
    return NULL;
  }

  char data[NVRAM_MAX_DATA] = {0};
  int data_len = 0;
  if (HMX_NVRAM_GetField(field->nvram_type, 0, data, sizeof(data)) != DRV_OK ||
      HMX_NVRAM_GetLength(field->nvram_type, &data_len) != DRV_OK) {
    return NULL;
  }
  char formatbuf[NVRAM_MAX_DATA * 2];
  char* nv = format_nvram(field->format, data, data_len, formatbuf, sizeof(formatbuf));
  if (quiet) {
    snprintf(output, outlen, "%s", nv);
  } else {
    snprintf(output, outlen, "%s=%s", name, nv);
  }
  return output;
}


// ----------------- WRITE NVRAM -----------------------------


unsigned char* parse_string(const char* input,
                            unsigned char* output, int* outlen) {
  int len = strlen(input);
  if (len > *outlen) {
    len = *outlen;
  }

  strncpy((char*)output, input, len);
  *outlen = len;
  return output;
}

unsigned char* parse_mac(const char* input,
                         unsigned char* output, int* outlen) {
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
                               unsigned char* output, int* outlen) {
  if (*outlen < 2) return NULL;

  if (sscanf(input, "%hhd.%hhd", &output[1], &output[0]) != 2) {
    return NULL;
  }
  *outlen = 2;
  return output;
}

unsigned char* parse_uint8(const char* input,
                           unsigned char* output, int* outlen) {
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
                               unsigned char* output, int* outlen) {
  int i, len = strlen(input) / 2;
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
                         unsigned char* output, int* outlen) {
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
                           unsigned char* output, int* outlen) {
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


int write_nvram(char* optarg) {
  char* equal = strchr(optarg, '=');
  if (equal == NULL) {
    return -1;
  }

  char* name = optarg;
  *equal = '\0';
  char* value = ++equal;

  const hnvram_field_t* field = get_nvram_field(name);
  if (field == NULL) {
    return -2;
  }

  unsigned char nvram_value[NVRAM_MAX_DATA];
  int nvram_len = sizeof(nvram_value);
  if (parse_nvram(field->format, value, nvram_value, &nvram_len) == NULL) {
    return -3;
  }

  if (HMX_NVRAM_SetField(field->nvram_type, 0,
                         nvram_value, nvram_len) != DRV_OK) {
    return -4;
  }

  return 0;
}

int hnvram_main(int argc, char * const argv[]) {
  DRV_Error err;

  libupgrade_verbose = 0;

  if ((err = HMX_NVRAM_Init()) != DRV_OK) {
    fprintf(stderr, "NVRAM Init failed: %d\n", err);
    exit(1);
  }

  char op = 0;     // operation
  int op_cnt = 0;  // operation
  int q_flag = 0;  // quiet: don't output name of variable.
  int b_flag = 0;  // binary: output the binary format
  char output[NVRAM_MAX_DATA];
  int c;
  while ((c = getopt(argc, argv, "dbqrw:")) != -1) {
    switch(c) {
      case 'b':
        b_flag = 1;
        break;
      case 'q':
        q_flag = 1;
        break;
      case 'w':
        {
          char* duparg = strdup(optarg);
          if (write_nvram(duparg) != 0) {
            fprintf(stderr, "Unable to write %s\n", duparg);
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
          if (read_nvram(argv[optind], output, sizeof(output), q_flag) == NULL) {
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
int main(int argc, char * const argv[]) {
  return hnvram_main(argc, argv);
}
#endif  // TEST_MAIN
