/* Copyright 2012 Google Inc. All Rights Reserved.
 * Author: weixiaofeng@google.com (Xiaofeng Wei)
 */

#include "sysvar.h"

static const unsigned long crc_table[256] = {
  0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
  0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
  0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
  0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
  0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
  0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
  0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
  0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
  0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
  0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
  0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
  0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
  0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
  0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
  0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
  0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
  0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
  0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
  0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
  0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
  0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
  0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
  0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
  0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
  0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
  0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
  0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
  0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
  0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
  0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
  0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
  0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
  0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
  0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
  0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
  0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
  0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
  0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
  0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
  0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
  0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
  0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
  0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
  0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
  0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
  0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
  0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
  0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
  0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
  0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
  0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
  0x2d02ef8dL
};

#define INIT_CRC  0xffffffffL

#define DO1(buf)  do {crc = crc_table[((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8);} while(0);
#define DO2(buf)  do {DO1(buf); DO1(buf);} while(0);
#define DO4(buf)  do {DO2(buf); DO2(buf);} while(0);
#define DO8(buf)  do {DO4(buf); DO4(buf);} while(0);

/*
 * sysvar_crc - calculate the checksum of data buffer
 */
static unsigned long sysvar_crc(unsigned char *buf, int len) {
  unsigned long crc = 0;

  crc = crc ^ INIT_CRC;
  while (len >= 8) {
    DO8(buf);
    len -= 8;
  }
  if (len) {
      do {
        DO1(buf);
    } while (--len);
  }
  return crc ^ INIT_CRC;
}

/*
 * sysvar_len - calculate the used bytes in data buffer
 */
static void sysvar_len(struct sysvar_buf *buf, int len) {
  buf->used_len += len;

  if (buf->used_len < 0)
    buf->used_len = 0;

  if (buf->used_len > buf->total_len)
    buf->used_len = buf->total_len;

  buf->free_len = buf->total_len - buf->used_len;
}

/*
 * sysvar_copy - copy data from/to the data buffer
 */
static void sysvar_copy(unsigned char *dst, unsigned char *src, int len,
                        unsigned char end) {
  int i;

  if (end == SYSVAR_STR_TO_BUF) {
    /* copy a string to data buffer */
    memset(dst, 0xff, len);
  } else if (end == SYSVAR_BUF_TO_STR) {
    /* copy data buffer to a string */
    memset(dst, 0, len + 1);
  } else {
    return;
  }

  for (i = 0; i < len; i++) {
    if (src[i] == end)
      break;
    dst[i] = src[i];
  }
}

/*
 * get_wc32 - return the write counter of data buffer
 */
unsigned long get_wc32(struct sysvar_buf *buf) {
  int len = buf->total_len;
  unsigned long x = buf->data[len + 3];

  x = (x << 8) + buf->data[len + 2];
  x = (x << 8) + buf->data[len + 1];
  x = (x << 8) + buf->data[len];
  return x;
}

/*
 * set_wc32 - update the write counter of data buffer
 */
void set_wc32(struct sysvar_buf *buf) {
  int len = buf->total_len;
  unsigned long wc = get_wc32(buf) + 1;

  buf->data[len + 3] = (unsigned char)(wc >> 24);
  buf->data[len + 2] = (unsigned char)(wc >> 16);
  buf->data[len + 1] = (unsigned char)(wc >> 8);
  buf->data[len] = (unsigned char)(wc);
}

/*
 * get_crc32 - check and return checksum of data buffer
 */
unsigned long get_crc32(struct sysvar_buf *buf) {
  int len = buf->total_len + SYSVAR_WC32;
  unsigned long x = buf->data[len + 3];

  x = (x << 8) + buf->data[len + 2];
  x = (x << 8) + buf->data[len + 1];
  x = (x << 8) + buf->data[len];
  return x;
}

/*
 * set_crc32 - update checksum of data buffer
 */
void set_crc32(struct sysvar_buf *buf) {
  int len = buf->total_len + SYSVAR_WC32;
  unsigned long crc = sysvar_crc(buf->data, buf->total_len);

  buf->data[len + 3] = (unsigned char)(crc >> 24);
  buf->data[len + 2] = (unsigned char)(crc >> 16);
  buf->data[len + 1] = (unsigned char)(crc >> 8);
  buf->data[len] = (unsigned char)(crc);
}

/*
 * load_var - move data from data buffer to data list
 */
int load_var(struct sysvar_buf *buf) {
  int i, len, ret;
  char name[SYSVAR_NAME + 1];
  char *value;

  /* clear system variables in data list */
  ret = clear_var(buf);

  /* system variable: name(32) + len(2) + value ... */
  for (i = 0; i < buf->total_len && ret == SYSVAR_SUCCESS; i += len) {
    if (buf->data[i] == 0xff)
      break;

    /* name of system variable */
    sysvar_copy((unsigned char *)name,
                &buf->data[i],
                SYSVAR_NAME,
                SYSVAR_BUF_TO_STR);
    i += SYSVAR_NAME;

    /* length of system variable */
    len = (buf->data[i] << 8) + buf->data[i + 1];
    if (len < 0 || len > buf->free_len)
      return SYSVAR_PARAM_ERR;

    value = (char *)malloc(len + 1);
    if (value == NULL)
      return SYSVAR_MEMORY_ERR;

    i += 2;
    /* copy system variable */
    sysvar_copy((unsigned char *)value, &buf->data[i], len,
                SYSVAR_BUF_TO_STR);

    /* add system variable to data list */
    ret = set_var(buf, name, value);

    free(value);
  }
  return ret;
}

/*
 * save_var - move data from data list to data buffer
 */
int save_var(struct sysvar_buf *buf) {
  int i, len;
  struct sysvar_list *curr = buf->list->next;

  /* clear data buffer */
  memset(buf->data, 0xff, buf->data_len);

  for (i = 0; i < buf->total_len && curr != NULL; i += len) {
    /* name of system variable */
    sysvar_copy(&buf->data[i], (unsigned char *)curr->name, SYSVAR_NAME,
                SYSVAR_STR_TO_BUF);
    i += SYSVAR_NAME;

    /* length of system variable */
    len = strlen(curr->value);
    if (len < 0 || len > buf->used_len)
      return SYSVAR_PARAM_ERR;

    buf->data[i] = (unsigned char)(len >> 8);
    buf->data[i + 1] = (unsigned char)len;
    i += 2;

    /* copy system variable */
    sysvar_copy(&buf->data[i], (unsigned char *)curr->value, len,
                SYSVAR_STR_TO_BUF);

    curr = curr->next;
  }

  return check_var(buf, SYSVAR_SET_MODE);
}

/*
 * get_var - return the system variable from data list
 */
int get_var(struct sysvar_list *var, char *name, char *value, int len) {
  if (len <= 0)
    goto get_err;

  /* copy variable name */
  strncpy(name, var->name, SYSVAR_NAME);

  /* copy variable value */
  if (var->value == NULL)
    goto get_err;

  strncpy(value, var->value, len);
  return SYSVAR_SUCCESS;

get_err:
  value[0] = '\0';
  return SYSVAR_GET_ERR;
}

/*
 * set_var - add the system variable from data buffer
 */
int set_var(struct sysvar_buf *buf, char *name, char *value) {
  struct sysvar_list *curr = buf->list;
  struct sysvar_list *var = NULL;
  int name_len = strlen(name);
  int value_len = strlen(value);

  /* system variable: name(32) + len(2) + value ... */
  if (name_len > SYSVAR_NAME)
    return SYSVAR_PARAM_ERR;

  var = (struct sysvar_list *)malloc(sizeof(struct sysvar_list));
  if (var == NULL)
    return SYSVAR_MEMORY_ERR;

  var->value = (char *)malloc(value_len + 1);
  if (var->value == NULL) {
    free(var);
    return SYSVAR_MEMORY_ERR;
  }

  strncpy(var->name, name, SYSVAR_NAME);
  strncpy(var->value, value, value_len);
  var->value[value_len] = '\0';
  var->len = SYSVAR_NAME + 2 + value_len;
  var->next = NULL;

  /* add system variable */
  while (curr->next != NULL)
    curr = curr->next;
  curr->next = var;

  /* update the used bytes in data buffer */
  sysvar_len(buf, var->len);
  return SYSVAR_SUCCESS;
}

/*
 * delete_var - delete system variable from data list
 */
int delete_var(struct sysvar_buf *buf, struct sysvar_list *var) {
  struct sysvar_list *curr = buf->list->next;

  /* last system variable? */
  if (curr == var) {
    buf->list->next = var->next;
  } else {
    /* go to next system variable */
    while (curr != NULL) {
      if (curr->next == var) {
        curr->next = var->next;
        goto delete_ok;
      }
      curr = curr->next;
    }
    return SYSVAR_DELETE_ERR;
  }

delete_ok:
  /* update the used bytes in data buffer */
  sysvar_len(buf, -var->len);

  if (var->value != NULL)
    free(var->value);
  free(var);

  return SYSVAR_SUCCESS;
}

/*
 * clear_var - delete all system variables in data list
 */
int clear_var(struct sysvar_buf *buf) {
  struct sysvar_list *curr = buf->list->next;
  struct sysvar_list *var;
  int ret = SYSVAR_SUCCESS;

  while (curr != NULL && ret == SYSVAR_SUCCESS) {
    /* store next position of variable */
    var = curr->next;
    /* delete last variable */
    ret = delete_var(buf, curr);
    /* restore next position of variable */
    curr = var;
  }
  return ret;
}

/*
 * find_var - find the system variable in data list
 */
struct sysvar_list *find_var(struct sysvar_buf *buf, char *name) {
  struct sysvar_list *curr = buf->list->next;

  if (name == NULL)
    return NULL;

  /* search the data list */
  while (curr != NULL) {
    if (strncmp(curr->name, name, SYSVAR_NAME) == 0)
      break;
    curr = curr->next;
  }
  return curr;
}

/*
 * check_var - check/update checksum and write counter in data buffer
 */
int check_var(struct sysvar_buf *buf, int mode) {
  unsigned long crc[2];
  int ret = SYSVAR_SUCCESS;

  switch (mode) {
    case SYSVAR_LOAD_MODE:
      crc[0] = get_crc32(buf);
      crc[1] = sysvar_crc(buf->data, buf->total_len);
      if (crc[0] == crc[1])
        buf->modified = false;
      else
        ret = SYSVAR_CRC_ERR;
      break;
    case SYSVAR_SAVE_MODE:
      set_wc32(buf);
      buf->modified = false;
      break;
    case SYSVAR_SET_MODE:
      set_crc32(buf);
      buf->modified = true;
      break;
    default:
      ret = SYSVAR_PARAM_ERR;
      break;
  }
  return ret;
}

/*
 * print_var - print the system variables
 */
void print_var(struct sysvar_buf *buf) {
  struct sysvar_list *curr = buf->list->next;

  while (curr != NULL) {
    /* print variable name */
    printf("%s\t%s\t%s\n", curr->name,
           buf->readonly ? "RO" : "RW", curr->value);
    curr = curr->next;
  }
}

/*
 * clear_buf - clear the data buffer
 */
void clear_buf(struct sysvar_buf *buf) {
  if (buf->data != NULL) {
    memset(buf->data, 0xff, buf->data_len);
    buf->loaded = true;

    set_wc32(buf);
    set_crc32(buf);
  }
}

/*
 * dump_buf - dump the data buffer in binary/ascii format
 */
void dump_buf(struct sysvar_buf *buf, int start, int len) {
  char c;
  int i, j;

  for (i = start; i < start + len; i += 16) {
    printf("[%08x] ", start + i);

    /* binary format */
    for (j = 0; j < 16; j++) {
      if (i + j < buf->data_len)
        printf("%02x ", buf->data[i + j]);
      else
        printf("   ");
    }

    /* ascii format */
    for (j = 0; j < 16; j++) {
      if (i + j < buf->data_len) {
        c = buf->data[i + j];
        if (c >= 32 && c < 127) {
          printf("%c", c);
        } else {
          printf(".");
        }
      } else {
        printf(" ");
      }
    }
    printf("\n");
  }
}

