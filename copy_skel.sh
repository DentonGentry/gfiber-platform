#!/bin/sh
set -x
TARGET_DIR=$1

cp -alf ${TARGET_DIR}/../build/bruno-HEAD/skel/. ${TARGET_DIR}/
