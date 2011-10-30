#!/bin/sh

TARGET_DIR=$1

rsync -aP ${TARGET_DIR}/../build/bruno-HEAD/bruno/gfhd100/skel/ ${TARGET_DIR}/
