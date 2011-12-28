#!/bin/sh

TARGET_DIR=$1
CWD=$(dirname $0)
${CWD}/copy_skel.sh ${TARGET_DIR}
install -m 0755 ${TARGET_DIR}/../build/bruno-HEAD/bruno/gfhd100/init.ramfs ${TARGET_DIR}/init
