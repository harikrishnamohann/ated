#! /usr/bin/env bash
if [[ $1 == "debug" ]]; then
  clang ./src/main.c -lncurses -std=gnu17 -Wall -fsanitize=address -g -o ated
elif [[ $1 == "release" ]]; then
  clang ./src/main.c -O2 -lncurses -std=gnu17 -o ated
else
  echo unknown compilation mode: $1
fi
