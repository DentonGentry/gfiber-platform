#!/bin/sh
#
# Copyright 2011 Google Inc. All Rights Reserved.
# Author: kedong@google.com (Ke Dong)

set -e

STAGING_DIR=$1/../staging

cd $1
cp -f ${STAGING_DIR}/usr/lib/humax/loader.bin .
gzip -9 < vmlinux > vmlinuz
${STAGING_DIR}/usr/lib/humax/makehdf ${STAGING_DIR}/usr/lib/bruno/lkr.cfg bruno.hdf

