#!/bin/bash

. ./wvtest/wvtest.sh

tmpdir="$(mktemp -d)"
export PATH="$tmpdir/bin:${PATH}"
export GINSTALL_HNVRAM_DIR="$tmpdir/hnvram"
export GINSTALL_OUT_FILE="$tmpdir/out"
export GINSTALL_PLATFORM_FILE="$tmpdir/etc/platform"
psiz=$(stat --format=%s testdata/img/loader.gflt110.bin)
lsiz=$(stat --format=%s testdata/img/loader.img)
ksiz=$(stat --format=%s testdata/img/kernel.img)
rsiz=$(stat --format=%s testdata/img/rootfs.img)
usiz=$(stat --format=%s testdata/img/uloader.img)
bsiz=$(stat --format=%s testdata/img/boot.img)
ssiz=$(stat --format=%s testdata/img/system.img.raw)
asiz=$(stat --format=%s testdata/img/android_bsu.elf)
testdata/bin/http_server "$tmpdir/http_ctrl" &

setup_fakeroot() {
  platform="$1"
  running_os="fiberos"
  [ $# -gt 1 ] && running_os="$2"
  rm -f "$GINSTALL_OUT_FILE"
  rm -rf "$tmpdir/*"
  mkdir -p "$tmpdir/bin" "$tmpdir/dev" "$tmpdir/etc"
  mkdir -p "$tmpdir/dev/block" "$tmpdir/dev/mtd"
  mkdir -p "$tmpdir/sys/block/sda"
  mkdir -p "$GINSTALL_HNVRAM_DIR"
  cp -r testdata/bin "$tmpdir"
  cp -r testdata/proc "$tmpdir"
  cp -r testdata/img "$tmpdir"
  cp -r testdata/sys "$tmpdir"

  # write a pre-existing uloader and bootloader
  echo 0123456789abcdef0123456789abcdef >"$tmpdir/dev/mtd0"
  echo 0123456789abcdef0123456789abcdef >"$tmpdir/dev/mtd1"
  echo 0123456789abcdef0123456789abcdef >"$tmpdir/dev/mtd2"
  echo 0123456789abcdef0123456789abcdef >"$tmpdir/dev/mtd3"
  echo 0123456789abcdef0123456789abcdef >"$tmpdir/dev/mtd4"

  # write a pre-existing android_bsu
  echo 0123456789abcdef0123456789abcdef >"$tmpdir/dev/mmcblk0p2"
  echo 0123456789abcdef0123456789abcdef >"$tmpdir/dev/block/mmcblk0p2"

  for i in {5..31}; do touch "$tmpdir/dev/mtd$i"; done

  # duplicate /dev/mtd* to /dev/mtd/mtd* (used by Android)
  cp ${tmpdir}/dev/mtd[0-9]* ${tmpdir}/dev/mtd/

  cp "testdata/proc/mtd.$platform" "$tmpdir/proc/mtd"
  echo "$platform" >"$tmpdir/etc/platform"
  echo "$running_os" >"$tmpdir/etc/os"
  echo 0123456789abcdef0123456789abcdef >"$tmpdir/etc/gfiber_public.der"
}


# kernel in NAND, raw no bbt
# rootfs in NAND, ubi
# loader in NOR, raw
# (GFHD100, GFMS100)
echo; echo; echo GFHD100
setup_fakeroot GFHD100
expected="\
psback
logos ginstall
ubidetach -p ${tmpdir}/dev/mtd13
ubiformat -y -q ${tmpdir}/dev/mtd13
ubiattach -p ${tmpdir}/dev/mtd13 -d 0
ubimkvol -N rootfs-prep -m /dev/ubi0
flash_erase --quiet ${tmpdir}/dev/mtd19 0 0
ubirename /dev/ubi0 rootfs-prep rootfs
ubidetach -d 0
flash_erase --quiet ${tmpdir}/dev/mtd11 0 0
flash_erase --quiet ${tmpdir}/dev/mtd0 0 0
hnvram -q -w ACTIVATED_KERNEL_NAME=kernel1"

WVPASS ./ginstall.py --basepath="$tmpdir" --tar=./testdata/img/image_v4.gi --partition=secondary --skiploadersig
WVPASSEQ "$expected" "$(cat $GINSTALL_OUT_FILE)"
WVPASS cmp --bytes="$lsiz" "${tmpdir}/dev/mtd0" testdata/img/loader.img
WVPASS cmp --bytes="$ksiz" "${tmpdir}/dev/mtd11" testdata/img/kernel.img
WVPASS cmp --bytes="$rsiz" "${tmpdir}/dev/mtd19" testdata/img/rootfs.img



# kernel in NAND, raw with bbt
# rootfs in NAND, ubi
# uloader in NOR, raw
# loader in NOR, raw
# (GFRG200, GFRG210)
echo; echo; echo GFRG210
setup_fakeroot GFRG210
expected="\
psback
logos ginstall
ubidetach -p ${tmpdir}/dev/mtd10
ubiformat -y -q ${tmpdir}/dev/mtd10
ubiattach -p ${tmpdir}/dev/mtd10 -d 0
ubimkvol -N rootfs-prep -m /dev/ubi0
flash_erase --quiet ${tmpdir}/dev/mtd16 0 0
nandwrite --quiet --markbad ${tmpdir}/dev/mtd16
nanddump --bb=skipbad --length=${rsiz} --quiet ${tmpdir}/dev/mtd16
ubirename /dev/ubi0 rootfs-prep rootfs
ubidetach -d 0
flash_erase --quiet ${tmpdir}/dev/mtd8 0 0
nandwrite --quiet --markbad ${tmpdir}/dev/mtd8
nanddump --bb=skipbad --length=${ksiz} --quiet ${tmpdir}/dev/mtd8
flash_erase --quiet ${tmpdir}/dev/mtd1 0 0
flash_erase --quiet ${tmpdir}/dev/mtd2 0 0
flash_erase --quiet ${tmpdir}/dev/mtd0 0 0
hnvram -q -w ACTIVATED_KERNEL_NAME=kernel1"

WVPASS ./ginstall.py --basepath="$tmpdir" --tar=./testdata/img/image_v4.gi --partition=secondary --skiploadersig
WVPASSEQ "$expected" "$(cat $GINSTALL_OUT_FILE)"
WVPASS cmp --bytes="$usiz" "${tmpdir}/dev/mtd0" testdata/img/uloader.img
WVPASS cmp --bytes="$lsiz" "${tmpdir}/dev/mtd1" testdata/img/loader.img
WVPASS cmp --bytes="$lsiz" "${tmpdir}/dev/mtd2" testdata/img/loader.img
WVPASS cmp --bytes="$ksiz" "${tmpdir}/dev/mtd8" testdata/img/kernel.img
WVPASS cmp --bytes="$rsiz" "${tmpdir}/dev/mtd16" testdata/img/rootfs.img



# kernel in NOR, raw
# rootfs on hard drive
# uloader in NOR, raw
# loader in NOR, raw
# (GFSC100)
echo; echo; echo GFSC100
setup_fakeroot GFSC100
echo nor >"${tmpdir}/sys/class/mtd/mtd7/type"
echo nor >"${tmpdir}/sys/class/mtd/mtd8/type"
expected="\
psback
logos ginstall
flash_erase --quiet ${tmpdir}/dev/mtd8 0 0
flash_erase --quiet ${tmpdir}/dev/mtd1 0 0
flash_erase --quiet ${tmpdir}/dev/mtd2 0 0
flash_erase --quiet ${tmpdir}/dev/mtd0 0 0
hnvram -q -w ACTIVATED_KERNEL_NAME=kernel1"

WVPASS ./ginstall.py --basepath="$tmpdir" --tar=./testdata/img/image_v4.gi --partition=secondary --skiploadersig
WVPASSEQ "$expected" "$(cat $GINSTALL_OUT_FILE)"
WVPASS cmp --bytes="$usiz" "${tmpdir}/dev/mtd0" testdata/img/uloader.img
WVPASS cmp --bytes="$lsiz" "${tmpdir}/dev/mtd1" testdata/img/loader.img
WVPASS cmp --bytes="$lsiz" "${tmpdir}/dev/mtd2" testdata/img/loader.img
WVPASS cmp --bytes="$ksiz" "${tmpdir}/dev/mtd8" testdata/img/kernel.img
WVPASS cmp --bytes="$rsiz" "${tmpdir}/dev/sda19" testdata/img/rootfs.img



# kernel in NAND, raw no bbt
# rootfs on eMMC
# (GFHD200)
echo; echo; echo GFHD200
setup_fakeroot GFHD200
expected="\
psback
logos ginstall
flash_erase --quiet ${tmpdir}/dev/mtd0 0 0
hnvram -q -w ACTIVATED_KERNEL_NAME=kernel1"

WVPASS ./ginstall.py --basepath="$tmpdir" --tar=./testdata/img/image_v4.gi --partition=secondary --skiploadersig
WVPASSEQ "$expected" "$(cat $GINSTALL_OUT_FILE)"
WVPASS cmp --bytes="$lsiz" "${tmpdir}/dev/mtd0" testdata/img/loader.img
WVPASS cmp --bytes="$ksiz" "${tmpdir}/dev/mmcblk0p18" testdata/img/kernel.img
WVPASS cmp --bytes="$rsiz" "${tmpdir}/dev/mmcblk0p19" testdata/img/rootfs.img



# kernel in NAND, raw no bbt
# rootfs on eMMC
# (GFHD254)
echo; echo; echo "GFHD254 (fiberos->fiberos)"
setup_fakeroot GFHD254 fiberos
expected="\
psback
logos ginstall
flash_erase --quiet ${tmpdir}/dev/mtd0 0 0
hnvram -q -w ACTIVATED_KERNEL_NAME=kernel1"

WVPASS ./ginstall.py --basepath="$tmpdir" --tar=./testdata/img/image_v4.gi --partition=secondary --skiploadersig
WVPASSEQ "$expected" "$(cat $GINSTALL_OUT_FILE)"
WVPASS cmp --bytes="$lsiz" "${tmpdir}/dev/mtd0" testdata/img/loader.img
WVPASS cmp --bytes="$ksiz" "${tmpdir}/dev/mmcblk0p14" testdata/img/kernel.img
WVPASS cmp --bytes="$rsiz" "${tmpdir}/dev/mmcblk0p15" testdata/img/rootfs.img

# FiberOS->Android (GFHD254)
echo; echo; echo "GFHD254 (fiberos->android)"
setup_fakeroot GFHD254 fiberos
expected="\
psback
logos ginstall
flash_erase --quiet ${tmpdir}/dev/mtd0 0 0
hnvram -q -w ANDROID_ACTIVE_PARTITION=b
hnvram -q -w BOOT_TARGET=android"

WVPASS ./ginstall.py --basepath="$tmpdir" --tar=./testdata/img/image_android_v4.gi --partition=secondary --skiploadersig
WVPASSEQ "$expected" "$(cat $GINSTALL_OUT_FILE)"
WVPASS cmp --bytes="$lsiz" "${tmpdir}/dev/mtd0" testdata/img/loader.img
WVPASS cmp --bytes="$asiz" "${tmpdir}/dev/mmcblk0p2" testdata/img/android_bsu.elf
WVPASS cmp --bytes="$bsiz" "${tmpdir}/dev/mmcblk0p6" testdata/img/boot.img
WVPASS cmp --bytes="$ssiz" "${tmpdir}/dev/mmcblk0p10" testdata/img/system.img.raw

# Android->Android (GFHD254)
echo; echo; echo "GFHD254 (android->android)"
setup_fakeroot GFHD254 android
expected="\
psback
logos ginstall
flash_erase --quiet ${tmpdir}/dev/mtd/mtd0 0 0
hnvram -q -w ANDROID_ACTIVE_PARTITION=a"

WVPASS ./ginstall.py --basepath="$tmpdir" --tar=./testdata/img/image_android_v4.gi --partition=primary --skiploadersig
WVPASSEQ "$expected" "$(cat $GINSTALL_OUT_FILE)"
WVPASS cmp --bytes="$lsiz" "${tmpdir}/dev/mtd/mtd0" testdata/img/loader.img
WVPASS cmp --bytes="$asiz" "${tmpdir}/dev/block/mmcblk0p2" testdata/img/android_bsu.elf
WVPASS cmp --bytes="$bsiz" "${tmpdir}/dev/block/mmcblk0p5" testdata/img/boot.img
WVPASS cmp --bytes="$ssiz" "${tmpdir}/dev/block/mmcblk0p9" testdata/img/system.img.raw

# Android->FiberOS (GFHD254)
echo; echo; echo "GFHD254 (android->fiberos)"
setup_fakeroot GFHD254 android
expected="\
psback
logos ginstall
flash_erase --quiet ${tmpdir}/dev/mtd/mtd0 0 0
hnvram -q -w ACTIVATED_KERNEL_NAME=kernel0
hnvram -q -w BOOT_TARGET=fiberos"

WVPASS ./ginstall.py --basepath="$tmpdir" --tar=./testdata/img/image_v4.gi --partition=primary --skiploadersig
WVPASSEQ "$expected" "$(cat $GINSTALL_OUT_FILE)"
WVPASS cmp --bytes="$lsiz" "${tmpdir}/dev/mtd/mtd0" testdata/img/loader.img
WVPASS cmp --bytes="$ksiz" "${tmpdir}/dev/block/mmcblk0p12" testdata/img/kernel.img
WVPASS cmp --bytes="$rsiz" "${tmpdir}/dev/block/mmcblk0p13" testdata/img/rootfs.img



# kernel in NOR, raw
# rootfs in NOR, raw
# loader in NOR, raw
# (GFMN100)
echo; echo; echo GFMN100
setup_fakeroot GFMN100
echo nor >"${tmpdir}/sys/class/mtd/mtd11/type"
echo nor >"${tmpdir}/sys/class/mtd/mtd12/type"
expected="\
psback
logos ginstall
flash_erase --quiet ${tmpdir}/dev/mtd11 0 0
flash_erase --quiet ${tmpdir}/dev/mtd12 0 0
flash_erase --quiet ${tmpdir}/dev/mtd0 0 0
hnvram -q -w ACTIVATED_KERNEL_NAME=kernel1"

WVPASS ./ginstall.py --basepath="$tmpdir" --tar=./testdata/img/image_v4.gi --partition=secondary --skiploadersig
WVPASSEQ "$expected" "$(cat $GINSTALL_OUT_FILE)"
WVPASS cmp --bytes="$lsiz" "${tmpdir}/dev/mtd0" testdata/img/loader.img
WVPASS cmp --bytes="$ksiz" "${tmpdir}/dev/mtd12" testdata/img/kernel.img
WVPASS cmp --bytes="$rsiz" "${tmpdir}/dev/mtd11" testdata/img/rootfs.img



# kernel in NOR, raw
# no separate rootfs (compiled into kernel)
# loader in NOR, raw
# GFLT110
echo; echo; echo GFLT110
setup_fakeroot GFLT110
expected="\
psback
logos ginstall
flash_unlock ${tmpdir}/dev/mtd6
flash_erase --quiet ${tmpdir}/dev/mtd6 0 0
flash_unlock ${tmpdir}/dev/mtd0
flash_erase --quiet ${tmpdir}/dev/mtd0 0 0
hnvram -q -w ACTIVATED_KERNEL_NAME=kernel0"

WVPASS ./ginstall.py --basepath="$tmpdir" --tar=./testdata/img/image_gflt110_v4.gi --partition=primary --skiploadersig
WVPASSEQ "$expected" "$(cat $GINSTALL_OUT_FILE)"
WVPASS cmp --bytes="$lsiz" "${tmpdir}/dev/mtd0" testdata/img/loader.img
WVPASS cmp --bytes="$ksiz" "${tmpdir}/dev/mtd6" testdata/img/kernel.img



# GFLT110 with gflt110.bin loader.
echo; echo; echo GFLT110 with platform loader
setup_fakeroot GFLT110
expected="\
psback
logos ginstall
flash_unlock ${tmpdir}/dev/mtd6
flash_erase --quiet ${tmpdir}/dev/mtd6 0 0
flash_unlock ${tmpdir}/dev/mtd0
flash_erase --quiet ${tmpdir}/dev/mtd0 0 0
hnvram -q -w ACTIVATED_KERNEL_NAME=kernel0"

WVPASS ./ginstall.py --basepath="$tmpdir" --tar=./testdata/img/image_gflt110_platform_loader.gi --partition=primary --skiploadersig
WVPASSEQ "$expected" "$(cat $GINSTALL_OUT_FILE)"
WVPASS cmp --bytes="$psiz" "${tmpdir}/dev/mtd0" testdata/img/loader.gflt110.bin
WVPASS cmp --bytes="$ksiz" "${tmpdir}/dev/mtd6" testdata/img/kernel.img



# GFLT110 with both loaders with the MANIFEST containing "multiloader: 1"
echo; echo; echo GFLT110 with both loaders
setup_fakeroot GFLT110
expected="\
psback
logos ginstall
flash_unlock ${tmpdir}/dev/mtd6
flash_erase --quiet ${tmpdir}/dev/mtd6 0 0
flash_unlock ${tmpdir}/dev/mtd0
flash_erase --quiet ${tmpdir}/dev/mtd0 0 0
hnvram -q -w ACTIVATED_KERNEL_NAME=kernel0"

WVPASS ./ginstall.py --basepath="$tmpdir" --tar=./testdata/img/image_gflt110_both_loaders.gi --partition=primary --skiploadersig
WVPASSEQ "$expected" "$(cat $GINSTALL_OUT_FILE)"
WVFAIL cmp --bytes="$lsiz" "${tmpdir}/dev/mtd0" testdata/img/loader.bin
WVPASS cmp --bytes="$psiz" "${tmpdir}/dev/mtd0" testdata/img/loader.gflt110.bin
WVPASS cmp --bytes="$ksiz" "${tmpdir}/dev/mtd6" testdata/img/kernel.img



echo; echo; echo MANIFEST with Bad checksums
setup_fakeroot GFHD100
echo "This should not be touched" >"${tmpdir}/dev/mtd0"
WVFAIL ./ginstall.py --basepath="$tmpdir" --tar=./testdata/img/image_v4_bad_checksums.gi --partition=secondary --skiploadersig
WVPASSEQ "This should not be touched" "$(cat ${tmpdir}/dev/mtd0)"



echo; echo; echo GFHD100 image v3
setup_fakeroot GFHD100
expected="\
psback
logos ginstall
ubidetach -p ${tmpdir}/dev/mtd13
ubiformat -y -q ${tmpdir}/dev/mtd13
ubiattach -p ${tmpdir}/dev/mtd13 -d 0
ubimkvol -N rootfs-prep -m /dev/ubi0
flash_erase --quiet ${tmpdir}/dev/mtd19 0 0
ubirename /dev/ubi0 rootfs-prep rootfs
ubidetach -d 0
flash_erase --quiet ${tmpdir}/dev/mtd11 0 0
flash_erase --quiet ${tmpdir}/dev/mtd0 0 0
hnvram -q -w ACTIVATED_KERNEL_NAME=kernel1"

WVPASS ./ginstall.py --basepath="$tmpdir" --tar=./testdata/img/image_v3.gi --partition=secondary --skiploadersig
WVPASSEQ "$expected" "$(cat $GINSTALL_OUT_FILE)"
WVPASS cmp --bytes="$lsiz" "${tmpdir}/dev/mtd0" testdata/img/loader.img
WVPASS cmp --bytes="$ksiz" "${tmpdir}/dev/mtd11" testdata/img/kernel.img
WVPASS cmp --bytes="$rsiz" "${tmpdir}/dev/mtd19" testdata/img/rootfs.img



echo; echo; echo GFHD100 image v2
setup_fakeroot GFHD100
expected="\
psback
logos ginstall
ubidetach -p ${tmpdir}/dev/mtd13
ubiformat -y -q ${tmpdir}/dev/mtd13
ubiattach -p ${tmpdir}/dev/mtd13 -d 0
ubimkvol -N rootfs-prep -m /dev/ubi0
flash_erase --quiet ${tmpdir}/dev/mtd19 0 0
ubirename /dev/ubi0 rootfs-prep rootfs
ubidetach -d 0
flash_erase --quiet ${tmpdir}/dev/mtd11 0 0
flash_erase --quiet ${tmpdir}/dev/mtd0 0 0
hnvram -q -w ACTIVATED_KERNEL_NAME=kernel1"

WVPASS ./ginstall.py --basepath="$tmpdir" --tar=./testdata/img/image_v2.gi --partition=secondary --skiploadersig
WVPASSEQ "$expected" "$(cat $GINSTALL_OUT_FILE)"
WVPASS cmp --bytes="$lsiz" "${tmpdir}/dev/mtd0" testdata/img/loader.img
WVPASS cmp --bytes="$ksiz" "${tmpdir}/dev/mtd11" testdata/img/kernel.img
WVPASS cmp --bytes="$rsiz" "${tmpdir}/dev/mtd19" testdata/img/rootfs.img



http_port=$(cat "$tmpdir/http_ctrl")
echo; echo; echo GFHD100 via HTTP
setup_fakeroot GFHD100
expected="\
psback
logos ginstall
ubidetach -p ${tmpdir}/dev/mtd13
ubiformat -y -q ${tmpdir}/dev/mtd13
ubiattach -p ${tmpdir}/dev/mtd13 -d 0
ubimkvol -N rootfs-prep -m /dev/ubi0
flash_erase --quiet ${tmpdir}/dev/mtd19 0 0
ubirename /dev/ubi0 rootfs-prep rootfs
ubidetach -d 0
flash_erase --quiet ${tmpdir}/dev/mtd11 0 0
flash_erase --quiet ${tmpdir}/dev/mtd0 0 0
hnvram -q -w ACTIVATED_KERNEL_NAME=kernel1"

url="http://127.0.0.1:${http_port}/testdata/img/image_v4.gi"
WVPASS ./ginstall.py --basepath="$tmpdir" --tar="$url" --partition=secondary --skiploadersig
WVPASSEQ "$expected" "$(cat $GINSTALL_OUT_FILE)"
WVPASS cmp --bytes="$lsiz" "${tmpdir}/dev/mtd0" testdata/img/loader.img
WVPASS cmp --bytes="$ksiz" "${tmpdir}/dev/mtd11" testdata/img/kernel.img
WVPASS cmp --bytes="$rsiz" "${tmpdir}/dev/mtd19" testdata/img/rootfs.img


echo 'quitquitquit' >"$tmpdir/http_ctrl"
wait
rm -rf "$tmpdir"
