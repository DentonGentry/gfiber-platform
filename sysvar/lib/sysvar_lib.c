/* Copyright 2012 Google Inc. All Rights Reserved.
 * Author: weixiaofeng@google.com (Xiaofeng Wei)
 */

#include "sysvarlib.h"

int mtd_dev[SYSVAR_MTD_DEVICE] = {-1, -1, -1, -1};
int mtd_dev_unlocked[SYSVAR_MTD_DEVICE];
bool verbose = false;
struct sysvar_buf rw_buf;
struct sysvar_buf ro_buf;
char *mtd_name[SYSVAR_MTD_DEVICE] = {
  SYSVAR_RW_NAME0, SYSVAR_RW_NAME1, SYSVAR_RO_NAME0, SYSVAR_RO_NAME1
};

/*
 * print_err - print the error message
 */
static void print_err(char *err, int idx) {
  if (verbose) {
    printf("error(sv): failed to %s", err);
    switch (idx) {
      case 0:
        printf("'%s'", SYSVAR_RW_NAME0);
        break;
      case 1:
        printf("'%s'", SYSVAR_RW_NAME1);
        break;
      case 2:
        printf("'%s'", SYSVAR_RO_NAME0);
        break;
      case 3:
        printf("'%s'", SYSVAR_RO_NAME1);
        break;
      default:
        break;
    }
    printf(" (%s)\n", strerror(errno));
  }
}

/*
 * check_mtd - check MTD device opened
 */
static int check_mtd(void) {
  int i;

  for (i = 0; i < SYSVAR_MTD_DEVICE; i++) {
    if (mtd_dev[i] < 0) {
      print_err("open MTD device ", i);
      return SYSVAR_OPEN_ERR;
    }
  }
  return SYSVAR_SUCCESS;
}

/*
 * erase_mtd - erase MTD device
 */
static int erase_mtd(int idx) {
  int res;
  struct mtd_info_user mi;
  struct erase_info_user ei;

  /* get MTD device information */
  if (ioctl(mtd_dev[idx], MEMGETINFO, &mi)) {
    print_err("getinfo MTD device ", idx);
    return SYSVAR_ERASE_ERR;
  }

  ei.start = 0;
  ei.length = mi.erasesize;
  while (ei.start < SYSVAR_BLOCK_SIZE) {
    /* For select devices, unlocking is not implemented.  So, if errno
     * is set to something like EOPNOTSUPP, it is not an error.  It is simply
     * not required to write/erase a block of the device. */
    res = ioctl(mtd_dev[idx], MEMUNLOCK, &ei);
    if (res != 0 && errno != EOPNOTSUPP) {
      print_err("unlock MTD device ", idx);
      return SYSVAR_ERASE_ERR;
    }

    /* Only mark the device as unlocked if the unlock was successful. */
    if (res == 0) {
      mtd_dev_unlocked[idx] = 1;
    }

    if (ioctl(mtd_dev[idx], MEMERASE, &ei)) {
      print_err("erase MTD device ", idx);
      return SYSVAR_ERASE_ERR;
    }

    /* move to next block */
    ei.start += ei.length;
  }
  return SYSVAR_SUCCESS;
}

/*
 * set_mtd_verbose - set verbose print
 */
void set_mtd_verbose(bool v) {
  verbose = v;
}

/*
 * data_recovery - system variables recovering routine
 */
static int data_recovery(struct sysvar_buf *buf, int idx) {
  int i, j, bytes;

  /* load the system vriables */
  for (i = idx, j = idx + 1; i < idx + 2; i++, j--) {
    /* read the data buffer from MTD device */
    lseek(mtd_dev[i], SYSVAR_MTD_OFFSET, SEEK_SET);
    bytes = read(mtd_dev[i], buf->data, buf->data_len);
    if (bytes != buf->data_len)
      continue;

    /* check crc32 and wc32 (write count) */
    if (check_var(buf, SYSVAR_LOAD_MODE) == SYSVAR_SUCCESS) {
      /* erase MTD device */
      if (erase_mtd(j))
        goto recovery_err;

      /* check crc32 and wc32 (write count) */
      if (check_var(buf, SYSVAR_SAVE_MODE))
        goto recovery_err;

      /* write the data buffer to MTD device */
      lseek(mtd_dev[j], SYSVAR_MTD_OFFSET, SEEK_SET);
      bytes = write(mtd_dev[j], buf->data, buf->data_len);
      if (bytes != buf->data_len) {
        print_err("write MTD device ", j);
        goto recovery_err;
      }

      buf->loaded = true;
      return SYSVAR_SUCCESS;
    }
  }

recovery_err:
  clear_buf(buf);

  print_err("recover MTD device ", idx);
  return SYSVAR_SUCCESS;
}

/*
 * data_load - load the data from MTD device to data buffer
 */
static int data_load(struct sysvar_buf *buf, int idx) {
  int i, j, bytes;

  buf->loaded = true;
  if (check_mtd())
    return SYSVAR_OPEN_ERR;

  /* load the system vriables */
  for (i = idx, j = 0; i < idx + 2; i++, j++) {
    buf->failed[j] = false;

    /* read the data buffer from MTD device */
    lseek(mtd_dev[i], SYSVAR_MTD_OFFSET, SEEK_SET);
    bytes = read(mtd_dev[i], buf->data, buf->data_len);
    if (bytes != buf->data_len)
      buf->failed[j] = true;

    /* check crc32 and wc32 (write count) */
    if (check_var(buf, SYSVAR_LOAD_MODE))
      buf->failed[j] = true;
  }

  if (buf->failed[0] || buf->failed[1])
    return data_recovery(buf, idx);
  return SYSVAR_SUCCESS;
}

/*
 * data_save - save the data from data buffer to MTD device
 */
static int data_save(struct sysvar_buf *buf, int *idx) {
  int i, j, bytes;

  if (check_mtd())
    return SYSVAR_OPEN_ERR;

  /* save the system vriables(RW) */
  for (j = 0; j < 2; j++) {
    i = idx[j];
    /* erase MTD device */
    if (erase_mtd(i))
      return SYSVAR_ERASE_ERR;

    /* check crc32 and wc32 (write count) */
    if (check_var(buf, SYSVAR_SAVE_MODE)) {
      print_err("save MTD device ", i);
      return SYSVAR_SAVE_ERR;
    }

    /* write the data buffer to MTD device */
    lseek(mtd_dev[i], SYSVAR_MTD_OFFSET, SEEK_SET);
    bytes = write(mtd_dev[i], buf->data, buf->data_len);
    if (bytes != buf->data_len) {
      print_err("write MTD device ", i);
      return SYSVAR_WRITE_ERR;
    }
  }
  return SYSVAR_SUCCESS;
}

/*
 * sv_buf - return the data buffer of system variables
 */
struct sysvar_buf *sv_buf(int idx) {
  if (idx < SYSVAR_RO_BUF)
    return &rw_buf;
  return &ro_buf;
}

/*
 * open_mtd - open MTD device and allocate the data buffer
 */
int open_mtd(void) {
  int i;

  /* check MTD devices */
  for (i = 0; i < SYSVAR_MTD_DEVICE; i++) {
    if (mtd_dev[i] >= 0)
      return SYSVAR_SUCCESS;
  }

  memset(&rw_buf, 0, sizeof(rw_buf));
  memset(&ro_buf, 0, sizeof(ro_buf));

  /* open MTD devices */
  for (i = 0; i < SYSVAR_MTD_DEVICE; i++) {
    mtd_dev[i] = open(mtd_name[i], O_RDWR | O_SYNC);
    if (mtd_dev[i] < 0) {
      print_err("open MTD device ", i);
      goto open_err;
    }
  }

  /* allocate data buffers */
  rw_buf.data = (unsigned char *)malloc(SYSVAR_BLOCK_SIZE);
  ro_buf.data = (unsigned char *)malloc(SYSVAR_BLOCK_SIZE);
  if (!rw_buf.data || !ro_buf.data) {
    print_err("allocate data buffer ", -1);
    goto open_err;
  }

  /* allocate data lists */
  rw_buf.list = (struct sysvar_list *)malloc(sizeof(struct sysvar_list));
  ro_buf.list = (struct sysvar_list *)malloc(sizeof(struct sysvar_list));
  if (!rw_buf.list || !ro_buf.list) {
    print_err("allocate data list ", -1);
    goto open_err;
  }

  rw_buf.data_len = SYSVAR_BLOCK_SIZE;
  rw_buf.total_len = SYSVAR_BLOCK_SIZE - SYSVAR_HEAD;
  rw_buf.free_len = rw_buf.total_len;
  rw_buf.readonly = false;

  strncpy(rw_buf.list->name, "rw", SYSVAR_NAME);
  rw_buf.list->value = NULL;
  rw_buf.list->len = SYSVAR_NAME + 2;
  rw_buf.list->next = NULL;

  ro_buf.data_len = SYSVAR_BLOCK_SIZE;
  ro_buf.total_len = SYSVAR_BLOCK_SIZE - SYSVAR_HEAD;
  ro_buf.free_len = ro_buf.total_len;
  ro_buf.readonly = true;

  strncpy(rw_buf.list->name, "ro", SYSVAR_NAME);
  ro_buf.list->value = NULL;
  ro_buf.list->len = SYSVAR_NAME + 2;
  ro_buf.list->next = NULL;

  if (!rw_buf.loaded || !ro_buf.loaded) {
    if (loadvar())
      goto open_err;
  }
  return SYSVAR_SUCCESS;

open_err:
  close_mtd();
  return SYSVAR_OPEN_ERR;
}

/*
 * close_mtd - close MTD device and release the data buffer
 */
void close_mtd(void) {
  int i;
  struct erase_info_user ei;

  /* release data lists */
  if (rw_buf.list != NULL) {
    clear_var(&rw_buf);
    free(rw_buf.list);
  }
  if (ro_buf.list != NULL) {
    clear_var(&ro_buf);
    free(ro_buf.list);
  }

  /* release data buffers */
  if (rw_buf.data != NULL)
    free(rw_buf.data);
  if (ro_buf.data != NULL)
    free(ro_buf.data);

  /* close MTD devices */
  for (i = 0; i < SYSVAR_MTD_DEVICE; i++) {
    if (mtd_dev[i] > 0) {
      if (mtd_dev_unlocked[i]) {
        ei.start = 0;
        ei.length = SYSVAR_BLOCK_SIZE;
        if (ioctl(mtd_dev[i], MEMLOCK, &ei))
          print_err("lock MTD device ", i);
        else
          mtd_dev_unlocked[i] = 0;
      }
      close(mtd_dev[i]);
    }
    mtd_dev[i] = -1;
  }
}

/*
 * loadvar - load the data from MTD device to data buffer
 */
int loadvar(void) {
  if (data_load(&rw_buf, SYSVAR_RW_BUF))
    return SYSVAR_LOAD_ERR;

  /* move the data from data buffer to data list */
  if (load_var(&rw_buf))
    return SYSVAR_LOAD_ERR;

  if (data_load(&ro_buf, SYSVAR_RO_BUF))
    return SYSVAR_LOAD_ERR;

  /* move the data from data buffer to data list */
  return load_var(&ro_buf);
}

/*
 * savevar - save the data from data buffer to MTD device(RW)
 */
int savevar(void) {
  int save_idx[2];

  /* move the data from data list to data buffer */
  if (save_var(&rw_buf))
    return SYSVAR_SAVE_ERR;

  /* erase failed partition first
   *  part0   part1       erase
   *  -----   -----       -----
   *    ok      ok        0, 1
   *  failed    ok        0, 1
   *    ok    failed      1, 0
   *  failed  failed      0, 1
   */
  if (rw_buf.failed[1]) {
    save_idx[0] = SYSVAR_RW_BUF + 1;
    save_idx[1] = SYSVAR_RW_BUF;
  } else {
    save_idx[0] = SYSVAR_RW_BUF;
    save_idx[1] = SYSVAR_RW_BUF + 1;
  }

  return data_save(&rw_buf, save_idx);
}

/*
 * getvar - get or print the system variable from data list
 */
int getvar(char *name, char *value, int len) {
  struct sysvar_list *var = NULL;

  if (check_mtd())
    return SYSVAR_OPEN_ERR;

  if (name == NULL) {
    /* print all system variables(RO) */
    print_var(&ro_buf);
    /* print all system variables(RW) */
    print_var(&rw_buf);
    return SYSVAR_SUCCESS;
  }

  /* find the system variable(RO) */
  var = find_var(&ro_buf, name);
  if (var != NULL)
    goto get_data;

  /* find the system variable(RW) */
  var = find_var(&rw_buf, name);
  if (var != NULL)
    goto get_data;

  /* system variable not found */
  return SYSVAR_GET_ERR;

get_data:
  return get_var(var, name, value, len);
}

/*
 * setvar - add or delete the system variable(RW) in data list
 */
int setvar(char *name, char *value) {
  struct sysvar_list *var = NULL;
  int ret = SYSVAR_SUCCESS;

  if (check_mtd())
    return SYSVAR_OPEN_ERR;

  if (name != NULL) {
    /* read only variable? */
    var = find_var(&ro_buf, name);
    if (var != NULL)
      return SYSVAR_READONLY_ERR;

    /* find system variable */
    var = find_var(&rw_buf, name);
    if (var != NULL) {
      /* delete system variable(RW) */
      ret = delete_var(&rw_buf, var);
      if (ret != SYSVAR_SUCCESS)
        return SYSVAR_DELETE_ERR;

      /* add system variable(RW) */
      if (value != NULL) {
        ret = set_var(&rw_buf, name, value);
      }
    } else {
      /* add system variable(RW) */
      if (value != NULL) {
        ret = set_var(&rw_buf, name, value);
      } else {
        ret = SYSVAR_EXISTED_ERR;
      }
    }
  } else {
    /* delete all of system variables(RW) */
    ret = clear_var(&rw_buf);
  }
  return ret;
}

/*
 * sysvar_info - get the system variables information
 */
void sysvar_info(int idx) {
  struct sysvar_buf *buf = sv_buf(idx);

  if (check_mtd())
    return;

  printf("System Variables(%s):\n", (idx < SYSVAR_RO_BUF) ? "RW" : "RO");
  if (idx == 0)
    printf("device : %s\n", SYSVAR_RW_NAME0);
  else if (idx == 1)
    printf("device : %s\n", SYSVAR_RW_NAME1);
  else if (idx == 2)
    printf("device : %s\n", SYSVAR_RO_NAME0);
  else if (idx == 3)
    printf("device : %s\n", SYSVAR_RO_NAME1);
  else
    printf("device : ?\n");
  printf("size   : %d bytes\n", buf->data_len);
  printf("total  : %d bytes\n", buf->total_len);
  printf("used   : %d bytes\n", buf->used_len);
  printf("wc32   : 0x%08lx\n", get_wc32(buf));
  printf("crc32  : 0x%08lx\n", get_crc32(buf));
}

/*
 * sysvar_dump - dump the data buffer in binary/ascii format
 */
void sysvar_dump(int idx, int start, int len) {
  if (check_mtd())
    return;

  dump_buf(sv_buf(idx), start, len);
}

/*
 * sysvar_io - MTD device IO operations
 */
int sysvar_io(int idx, int mode) {
  struct sysvar_buf *buf = sv_buf(idx);
  int bytes;

  if (check_mtd())
    return SYSVAR_OPEN_ERR;

  if (mode == SYSVAR_MTD_WRITE) {
    /* write the data buffer to MTD device */
    lseek(mtd_dev[idx], SYSVAR_MTD_OFFSET, SEEK_SET);
    bytes = write(mtd_dev[idx], buf->data, buf->data_len);
    if (bytes != buf->data_len) {
      print_err("write MTD device ", idx);
      return SYSVAR_WRITE_ERR;
    }
  } else if (mode == SYSVAR_MTD_ERASE) {
    /* erase MTD device */
    if (erase_mtd(idx))
      return SYSVAR_ERASE_ERR;
  }

  /* read the data buffer from MTD device */
  lseek(mtd_dev[idx], SYSVAR_MTD_OFFSET, SEEK_SET);
  bytes = read(mtd_dev[idx], buf->data, buf->data_len);
  if (bytes != buf->data_len) {
    print_err("read MTD device ", idx);
    return SYSVAR_READ_ERR;
  }

  rw_buf.loaded = false;
  ro_buf.loaded = false;
  return SYSVAR_SUCCESS;
}

