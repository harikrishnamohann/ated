#! /usr/bin/env bash
if [[ $1 == "debug" ]]; then
  clang ./src/main.c -lncurses -std=c11 -Wall -Werror -fsanitize=address -g -o ated
elif [[ $1 == "release" ]]; then
  clang ./src/main.c -O2 -lncurses -std=c11 -o ated
else
  echo unknown compilation mode: $1
fi
