/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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
#include <assert.h>
#include <fcntl.h>
#include <mtd/mtd-abi.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <Python.h>


static PyObject *
eccstats(PyObject *self, PyObject *args, PyObject *kwargs)
{
  char *kwlist[] = {"mtd", NULL};
  char *mtd = NULL;
  struct mtd_ecc_stats stats;
  int fd, rc;

  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s", kwlist, &mtd)) {
    PyErr_SetString(PyExc_TypeError, "bad arguments");
    return NULL;
  }

  if ((fd = open(mtd, 0)) < 0) {
    char err[80];
    snprintf(err, sizeof(err), "No such MTD device %s", mtd);
    PyErr_SetString(PyExc_IOError, err);
    return NULL;
  }

  rc = ioctl(fd, ECCGETSTATS, &stats);
  close(fd);

  if (rc) {
    PyErr_SetString(PyExc_OSError, "ioctl ECCGETSTATS failed");
    return NULL;
  }

  return Py_BuildValue("(iiii)", stats.corrected, stats.failed,
      stats.badblocks, stats.bbtblocks);
}

static PyMethodDef py_mtd_methods[] = {
  { "eccstats", (PyCFunction)eccstats, METH_VARARGS | METH_KEYWORDS,
      "Return a tuple of (corrected, failed, badblocks, bbtblocks)." },
  { NULL, NULL, 0, NULL },  // sentinel
};

PyMODINIT_FUNC init_py_mtd(void)
{
  PyObject *m;

  m = Py_InitModule("_py_mtd", py_mtd_methods);
  if (NULL == m) {
    return;
  }
}
