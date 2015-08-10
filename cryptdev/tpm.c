/*
 * Copyright 2015 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <dbus/dbus.h>

#include "tpm.h"


#define CRYPTOHOME_NAME "org.chromium.Cryptohome"
#define CRYPTOHOME_PATH "/org/chromium/Cryptohome"
#define CRYPTOHOME_INTERFACE "org.chromium.CryptohomeInterface"

#define CRYPTOHOME_TPM_DECRYPT "TpmDecrypt"
#define CRYPTOHOME_TPM_ENCRYPT "TpmEncrypt"

static dbus_bool_t check_err(DBusError* err, const char* msg) {
  if (dbus_error_is_set(err)) {
    fprintf(stderr, "ERROR: %s: %s\n", msg, err->message);
    dbus_error_free(err);
    return FALSE;
  }
  return TRUE;
}

static DBusHandlerResult filter(
    DBusConnection *conn, DBusMessage *msg, void *user_data) {
  DBusError err;
  const char* name;
  dbus_bool_t* found = (dbus_bool_t*) user_data;

  if (!dbus_message_is_signal(msg, DBUS_INTERFACE_DBUS, "NameOwnerChanged")) {
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  dbus_error_init(&err);
  if (!dbus_message_get_args(
      msg, &err, DBUS_TYPE_STRING, &name, DBUS_TYPE_INVALID)) {
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  if (strcmp(name, CRYPTOHOME_NAME) == 0) {
    *found = TRUE;
  }

  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static dbus_bool_t found = FALSE;

static dbus_bool_t wait_for_service(DBusConnection* conn) {
  DBusError err;
  const char* match =
      "interface=" DBUS_INTERFACE_DBUS
      ",member=NameOwnerChanged,"
      "arg0=" CRYPTOHOME_NAME;

  dbus_error_init(&err);
  dbus_bus_add_match(conn, match, &err);
  if (!check_err(&err, "add_match")) {
    return FALSE;
  }

  if (!dbus_connection_add_filter(conn, filter, &found, NULL)) {
    return FALSE;
  }

  dbus_error_init(&err);
  if (dbus_bus_name_has_owner(conn, CRYPTOHOME_NAME, &err)) {
    goto cleanup;
  }

  while (!found && dbus_connection_read_write_dispatch (conn, 10000)) {}

cleanup:
  dbus_connection_remove_filter(conn, filter, &found);
  return TRUE;
}

static DBusMessage* make_msg(const char* method) {
  return dbus_message_new_method_call(
      CRYPTOHOME_NAME, // target
      CRYPTOHOME_PATH, // object
      CRYPTOHOME_INTERFACE, // interface
      method); // method
}

static DBusPendingCall* send_msg(DBusConnection* conn, DBusMessage* msg) {
  DBusPendingCall* pending = NULL;

  if (!dbus_connection_send_with_reply(conn, msg, &pending, -1)) {
    return NULL;
  }

  dbus_connection_flush(conn);

  return pending;
}

static DBusMessage* wait_reply(DBusConnection* conn, DBusPendingCall* pending) {
  DBusMessage* msg;
  dbus_pending_call_block(pending);
  msg = dbus_pending_call_steal_reply(pending);
  dbus_pending_call_unref(pending);
  return msg;
}

static dbus_bool_t cipher_data(
    DBusConnection* conn,
    dbus_bool_t encrypt,
    void* input, size_t input_size,
    void** output, size_t* output_size) {
  DBusError err;
  DBusMessage* msg;
  DBusPendingCall* pending;
  dbus_bool_t ok = FALSE;

  msg = make_msg(encrypt ? CRYPTOHOME_TPM_ENCRYPT : CRYPTOHOME_TPM_DECRYPT);

  if (!dbus_message_append_args(
      msg,
      DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, &input, input_size,
      DBUS_TYPE_INVALID)) {
    return -1;
  }

  pending = send_msg(conn, msg);
  if (!pending) {
    dbus_message_unref(msg);
    return -1;
  }

  msg = wait_reply(conn, pending);

  dbus_error_init(&err);
  if (!dbus_message_get_args(
      msg, &err,
      DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE, output, output_size,
      DBUS_TYPE_BOOLEAN, &ok,
      DBUS_TYPE_INVALID)) {
    if (dbus_set_error_from_message(&err, msg)) {
      check_err(&err, "cipher_data");
    }
    ok = FALSE;
  }

  dbus_message_unref(msg);
  return ok ? 0 : -1;
}

int tpm_decrypt(
    tpm_handle_t h,
    void* input, size_t input_size,
    void** output, size_t* output_size) {
  DBusConnection* conn = (DBusConnection*) h;
  return (int) cipher_data(conn, FALSE, input, input_size, output, output_size);
}

int tpm_encrypt(
    tpm_handle_t h,
    void* input, size_t input_size,
    void** output, size_t* output_size) {
  DBusConnection* conn = (DBusConnection*) h;
  return (int) cipher_data(conn, TRUE, input, input_size, output, output_size);
}

tpm_handle_t tpm_open() {
  DBusError err;
  DBusConnection* conn;

  dbus_error_init(&err);
  conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
  if (!check_err(&err, "dbus_bus_get")) {
    return NULL;
  }

  if (!wait_for_service(conn)) {
    return NULL;
  }

  return (tpm_handle_t) conn;
}

void tpm_close(tpm_handle_t h) {
  DBusConnection* conn = (DBusConnection*) h;
  dbus_connection_unref(conn);
}

int tpm_read_random(void* buf, size_t count) {
  int ret = -1;
  int bytes;

  int fd = open("/dev/urandom", O_RDONLY);
  if (fd < 0) {
    return -1;
  }

  for (bytes = 0; bytes < count; ) {
    int r = read(fd, buf + bytes, count - bytes);
    if (r < 0) {
      break;
    }
    bytes += r;
  }

  if (bytes == count) {
    ret = 0;
  }

cleanup:
  close(fd);
  return ret;
}
