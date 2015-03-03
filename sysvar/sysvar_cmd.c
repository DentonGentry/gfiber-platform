/* Copyright 2012 Google Inc. All Rights Reserved.
 * Author: zixia@google.com (Ted Huang)
 *         weixiaofeng@google.com (Xiaofeng Wei)
 */

#include <termios.h>
#include <getopt.h>

#include "sysvarlib.h"

#define PAGE_SIZE           256
#define SYSVAR_VALUE        2048

#define READ_CMD            1
#define WRITE_CMD           2
#define ERASE_CMD           3

#define CMD_STR_NUM         12
#define STRING_LEN          20

#define ERROR_MSG           "<<ERROR CODE>>: "

bool debug = false;

typedef enum CmdType {
    cmdtype_clear   = 0,
    cmdtype_del     = 1,
    cmdtype_dump    = 2,
    cmdtype_erase   = 3,
    cmdtype_exit    = 4,
    cmdtype_get     = 5,
    cmdtype_load    = 6,
    cmdtype_print   = 7,
    cmdtype_read    = 8,
    cmdtype_save    = 9,
    cmdtype_set     = 10,
    cmdtype_write   = 11,
} CmdType;

char *cmd_str[CMD_STR_NUM] = {
  "clear", "del", "dump", "erase",
  "exit", "get", "load", "print",
  "read", "save", "set", "write"
};

/*
 * print_usage - print commandline usage messages
 */
void print_usage(char *cmd) {
  printf("usage: %s\n", cmd);
  printf("       %s --debug\n", cmd);
  printf("       %s --print\n", cmd);
  printf("       %s --clear\n", cmd);
  printf("       %s --get var_name\n", cmd);
  printf("       %s --remove var_name\n", cmd);
  printf("       %s --set var_name var_value\n", cmd);
}

/*
 * print_console_help - print help messages in sysvar_cmd console
 */
void print_console_help(bool debug) {
  printf("command:\n"
         "  load.....load system variables to data buffer\n"
         "  save.....save system variables to MTD device\n"
         "  print....print system variables\n"
         "  dump.....dump data in data buffer\n"
         "  get......get system variable\n"
         "  set......add/set system variable\n"
         "  del......delete system variable\n"
         "  clear....delete all system variables\n");

  if (debug) {
    printf("\n"
            "  read.....read data from MTD device\n"
            "  write....write data to MTD device\n"
            "  erase....erase MTD device\n");
  }

  printf("  exit.....exit sysvar_cmd application\n");
}

/*
 * get_str - get input string from STDIN
 */
void get_str(char *name, char *str, int len) {
  int c, i = 0;
  struct termios tms0, tms1;

  printf("%s > ", name);

  tcgetattr(STDIN_FILENO, &tms0);
  tms1 = tms0;
  tms1.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &tms1);

  while (1) {
    c = getchar();
    if (c == '\n') {
      putchar('\r');
      putchar('\n');
      str[i] = '\0';
      break;
    } else if (c == 0x08) {
      if (i > 0) {
        putchar('\b');
        putchar(' ');
        putchar('\b');
        i--;
      }
    } else if (c >= 0x20 && c < 0x7F) {
      if (i < len - 1) {
        putchar(c);
        str[i++] = c;
      }
    }
  }

  tcsetattr(STDIN_FILENO, TCSANOW, &tms0);
}

/*
 * print_msg - put output string to STDOUT
 */
void print_msg(char *msg, int idx) {
  if (debug) {
    printf("> System variables(%s): %s\n",
      (idx < SYSVAR_RO_BUF) ? "RW" : "RO", msg);
  }
}

/*
 * load_cmd - load system variables from MTD devices
 */
int load_cmd() {
  int ret;

  ret = loadvar();
  if (ret == SYSVAR_SUCCESS) {
    print_msg("loaded", SYSVAR_RO_BUF);
    print_msg("loaded", SYSVAR_RW_BUF);
  }
  return ret;
}

/*
 * save_cmd - save system variables(RW) to MTD devices
 */
int save_cmd() {
  int ret;

  ret = savevar();
  if (ret == SYSVAR_SUCCESS)
    print_msg("saved", SYSVAR_RW_BUF);
  return ret;
}

/*
 * print_cmd - print all system variables
 * getvar command:
 *    getvar name - get a system variable
 *    getvar      - print all system variables
 */
int print_cmd() {
  int ret;
  struct sysvar_buf *buf;
  char str[STRING_LEN];

  /* print all system variables */
  ret = getvar(NULL, NULL, 0);

  buf = sv_buf(SYSVAR_RO_BUF);
  sprintf(str, "%d/%d bytes", buf->used_len, buf->total_len);
  print_msg(str, SYSVAR_RO_BUF);

  buf = sv_buf(SYSVAR_RW_BUF);
  sprintf(str, "%d/%d bytes", buf->used_len, buf->total_len);
  print_msg(str, SYSVAR_RW_BUF);

  if (ret != SYSVAR_SUCCESS)
    fprintf(stderr, "%s%d\n", ERROR_MSG, ret);
  return ret;
}

/*
 * get_cmd - get system variable in data buffer
 * getvar command:
 *    getvar name - get a system variable
 *    getvar      - print all system variables
 */
int get_cmd(char *name) {
  int ret = SYSVAR_SUCCESS;
  char value[SYSVAR_VALUE];

  if (name[0] == '\0')
    ret = print_cmd();
  else {
    ret = getvar(name, value, SYSVAR_VALUE);
    if (ret == SYSVAR_SUCCESS)
      printf("%s\n", value);
    else
      fprintf(stderr, "%s%d\n", ERROR_MSG, ret);
  }
  return ret;
}

/*
 * clear_cmd - delete all system variables in data buffer
 * setvar command:
 *    setvar name value     - add a system variable(RW)
 *    setvar name           - delete a system variable(RW)
 *    setvar                - delete all system variables(RW)
 */
int clear_cmd() {
  int ret;

  ret = setvar(NULL, NULL);
  if (ret == SYSVAR_SUCCESS)
    print_msg("deleted", SYSVAR_RW_BUF);
  else
    fprintf(stderr, "%s%d\n", ERROR_MSG, ret);
  return ret;
}

/*
 * delete_cmd - delete a system variable in data buffer
 * setvar command:
 *    setvar name value - add a system variable(RW)
 *    setvar name       - delete a system variable(RW)
 *    setvar            - delete all system variables(RW)
 */
int delete_cmd(char *name) {
  int ret;

  ret = setvar(name, NULL);
  if (ret == SYSVAR_SUCCESS)
    print_msg("deleted", SYSVAR_RW_BUF);
  else
    fprintf(stderr, "%s%d\n", ERROR_MSG, ret);
  return ret;
}

/*
 * set_cmd - add/set system variable in data buffer
 * setvar command:
 *    setvar name value - add a system variable(RW)
 *    setvar name       - delete a system variable(RW)
 *    setvar            - delete all system variables(RW)
 */
int set_cmd(char *name, char *value) {
  int ret;

  ret = setvar(name, value);
  if (ret == SYSVAR_SUCCESS) {
    print_msg("added", SYSVAR_RW_BUF);
    printf("%s\n", value);
  }
  else
    fprintf(stderr, "%s%d\n", ERROR_MSG, ret);
  return ret;
}


/*
 * dump_data - dump data buffer in binary/ascii format
 */
void dump_data(int idx) {
  struct sysvar_buf *buf = sv_buf(idx);
  char str[2];
  int start = 0;

  sysvar_info(idx);
  while (1) {
    /* dump one page data in data buffer */
    sysvar_dump(idx, start, PAGE_SIZE);
    /* continue to dump...? */
    get_str("(n)ext, (p)rev, (f)irst, (l)ast ?", str, 2);
    if (strcmp("n", str) == 0) {
      start += PAGE_SIZE;
      if (start >= buf->data_len)
        return;
    } else if (strcmp("p", str) == 0) {
      start -= PAGE_SIZE;
      if (start < 0)
        return;
    } else if (strcmp("f", str) == 0) {
      if (start == 0)
        return;
      start = 0;
    } else if (strcmp("l", str) == 0) {
      if (start == buf->data_len - PAGE_SIZE)
        return;
      start = buf->data_len - PAGE_SIZE;
    } else {
      return;
    }
  }
}

/*
 * data_cmd - read/write/erase MTD device
 */
int data_cmd(char *name, int cmd, bool debug) {
  struct sysvar_buf *buf;
  char str[2];
  int i, ret, idx;

  /* debug only commands */
  if (!debug) {
    print_console_help(false);
    return SYSVAR_DEBUG_ERR;
  }

  get_str("mtd 2|3|4|5 ?", str, 2);
  if (str[0] == '2') {
     idx = 0;
  } else if (str[0] == '3') {
     idx = 1;
  } else if (str[0] == '4') {
    idx = 2;
  } else if (str[0] == '5') {
    idx = 3;
  } else {
    printf("Error: invalid MTD device\n");
    return SYSVAR_DEBUG_ERR;
  }

  buf = sv_buf(idx);

  printf("%s(%d): ", name, idx);
  switch (cmd) {
    case READ_CMD:
      ret = sysvar_io(idx, SYSVAR_MTD_READ);
      break;
    case WRITE_CMD:
      /* fill test pattern to data buffer */
      for (i = 0; i < buf->data_len; i++)
        buf->data[i] = i;
      ret = sysvar_io(idx, SYSVAR_MTD_WRITE);
      break;
    case ERASE_CMD:
      ret = sysvar_io(idx, SYSVAR_MTD_ERASE);
      break;
    default:
      ret = SYSVAR_PARAM_ERR;
      break;
  }

  if (ret == SYSVAR_SUCCESS) {
    printf("success\n");
    /* dump data in data buffer */
    dump_data(idx);
  } else {
    printf("failed\n");
  }

  return ret;
}

/*
 * cmd_console_wrapper - sysvar console wrapper
 */
void cmd_console_wrapper(char *cmd) {
  char name[SYSVAR_NAME];
  char value[SYSVAR_VALUE];

  get_str("name ?", name, SYSVAR_NAME);
  if (name[0] == '\0') {
    clear_cmd();
  } else {
    /* add or delete? */
    if (strcmp(cmd, "set") == 0)
      get_str("value ?", value, SYSVAR_VALUE);
    else
      value[0] = '\0';

    if (strcmp(cmd, "get") == 0)
      get_cmd(name);
    else if (value[0] == '\0')
      delete_cmd(name);
    else
      set_cmd(name, value);
  }
}

/*
 * get_cmd_index - This gets the command index
 */
int get_cmd_index(char str[]) {
  int cmd_index;

  for (cmd_index = 0; cmd_index < CMD_STR_NUM; cmd_index++) {
    if (strcmp(cmd_str[cmd_index], str) == 0)
      break;
  }

  return cmd_index;
}

/*
 * run_cmd - This runs the sysvar command line
 */
bool run_cmd(int cmd_index, bool debug) {
  bool done = false;

  switch (cmd_index) {
    case cmdtype_clear:   /* delete all system variables(RW) */
      clear_cmd();
      break;
    case cmdtype_del:   /* delete system variable(RW) */
      cmd_console_wrapper("del");
      break;
    case cmdtype_dump:   /* dump data in data buffer */
      dump_data(SYSVAR_RW_BUF);
      break;
    case cmdtype_erase:  /* erase data on MTD device */
      data_cmd("read_cmd", ERASE_CMD, debug);
      break;
    case cmdtype_exit:  /* exit */
      done = true;
      break;
    case cmdtype_get:   /* get system variables */
      cmd_console_wrapper("get");
      break;
    case cmdtype_load:   /* load system variables */
      load_cmd();
      break;
    case cmdtype_print:   /* print system variables */
      print_cmd();
      break;
    case cmdtype_read:   /* read data from MTD device */
      data_cmd("read_cmd", READ_CMD, debug);
      break;
    case cmdtype_save:   /* save system variables(RW) */
      save_cmd();
      break;
    case cmdtype_set:   /* add system variable(RW) */
      cmd_console_wrapper("set");
      break;
    case cmdtype_write:   /* write data to MTD device */
      data_cmd("read_cmd", WRITE_CMD, debug);
      break;
    default:
      print_console_help(debug);
      break;
  }

  return done;
}

/*
 * run_console - This runs the sysvar console
 */
void run_console(bool debug) {
  bool done = false;
  char str[8];
  int cmd_index;

  while (!done) {
    if (debug)
      get_str("sysvar_cmd(d)", str, 8);
    else
      get_str("sysvar_cmd", str, 8);

    cmd_index = get_cmd_index(str);
    done = run_cmd(cmd_index, debug);
  }
}

/*
 * assert_usage - This asserts the correct usage
 */
void assert_usage(int argc, int correct, char *argv[]) {
  if (argc != correct) {
    print_usage(argv[0]);
    exit(1);
  }
}

/*
 * main - This is a demo code to explain the usage of sysvar library
 */
int main(int argc, char *argv[]) {
  int c;
  int ret_val = 0;

  if (argc > 5) {
    print_usage(argv[0]);
    return 1;
  }

  /* open MTD devices to load system variables */
  if (open_mtd())
    return 1;

  if (argc == 1) {
    run_console(debug);
    return 0;
  }

  while (1)
  {
    static struct option long_options[] =
    {
      {"debug",   no_argument,       0, 'd'},
      {"print",   no_argument,       0, 'p'},
      {"clear",   no_argument,       0, 'c'},
      {"get",     required_argument, 0, 'g'},
      {"remove",  required_argument, 0, 'r'},
      {"set",     required_argument, 0, 's'},
      {0, 0, 0, 0}
    };

    int option_index = 0;

    c = getopt_long (argc, argv, "dpcg:r:s:",
                    long_options, &option_index);

    if (c == -1)  break;

    switch (c) {
      case 'd':
        assert_usage(argc, 2, argv);
        debug = true;
        set_mtd_verbose(debug);
        run_console(debug);
        break;

      case 'p':
        assert_usage(argc, 2, argv);
        ret_val = print_cmd();
        break;

      case 'c':
        assert_usage(argc, 2, argv);
        ret_val = clear_cmd();
        break;

      case 'g':
        assert_usage(argc, 3, argv);
        ret_val = get_cmd(optarg);
        break;

      case 'r':
        assert_usage(argc, 3, argv);
        ret_val = delete_cmd(optarg);
        break;

      case 's':
        assert_usage(argc, 4, argv);
        ret_val = set_cmd(optarg, argv[3]);
        break;

      default:
        print_usage(argv[0]);
        return 1;
    }

    if ((c != 'p') && (c != 'g'))
      save_cmd();
  }

  /* close MTD devices and release data buffer */
  close_mtd();
  if (ret_val != 0)
    return 1;
  return 0;
}
