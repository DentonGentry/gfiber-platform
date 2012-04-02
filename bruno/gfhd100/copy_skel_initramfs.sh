#!/bin/sh
set -x
TARGET_DIR=$1
CWD=$(dirname $0)
${CWD}/copy_skel.sh ${TARGET_DIR}
ln -f ${TARGET_DIR}/../build/bruno-HEAD/bruno/gfhd100/init.ramfs ${TARGET_DIR}/init
chmod 0755 ${TARGET_DIR}/init
