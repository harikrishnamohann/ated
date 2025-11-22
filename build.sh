#! /usr/bin/env bash

SRC="./src/main.c ./src/editor.c"
TARGET="edy"
CC=clang
CFLAGS="-lncurses -std=gnu17"

if [[ $1 == "debug" ]]; then
  $CC $SRC $CFLAGS -Wall -fsanitize=address -g -o $TARGET
elif [[ $1 == "release" ]]; then
  $CC $SRC -O2 $CFLAGS -o $TARGET
else
  echo unknown compilation mode: $1
fi
