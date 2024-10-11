#!/bin/sh

CRAP_DEFS=""
CRAP_FLAGS=""


if [ "z$1" = "z-h" ]; then
  echo "there is only one option: --crapberra"
  echo "it means: use libcanberra (whatever it is)"
  exit 1
elif [ "z$1" = "z--crapberra" ]; then
  CRAP_DEFS="-DUSE_CRAPBERRA"
  CRAP_FLAGS="-ldl"
elif [ "z$1" != "z" ]; then
  echo "wut? use -h!"
  exit 1
fi


pkg-config --exists x11
res=$?
if [ "$res" -ne 0 ]; then
  echo "i want Xlib!"
  exit 1
fi

XLIB_FLAGS=`pkg-config --cflags --libs x11`

odir=`pwd`
mdir=`dirname "$0"`
cd "$mdir"

CFLAGS="$CFLAGS -fwrapv -fno-delete-null-pointer-checks -fno-strict-aliasing -fno-diagnostics-show-caret"
if [ "z$GCC" = "z" ]; then
  GCC="gcc"
fi

$GCC -std=gnu99 -O2 -Wall $CFLAGS -s src/yterm_main.c -o yterm $CRAP_DEFS $XLIB_FLAGS $CRAP_FLAGS -lrt -lutil
res=$?

cd "$odir"
exit $res
