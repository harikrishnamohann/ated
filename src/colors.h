#pragma once
#include "include/itypes.h"
#include <ncurses.h>

enum {
  EDITOR_PAIR = 1,
  STATLN_PAIR,
  LNO_PAIR,
  TEST_PAIR,
};

#define bg 0
#define fg 1

i16 editor[2], statln[2], lno[2], test[2];

typedef enum {
  default_light,
} Theme;

static void set_default_light() {
  editor[bg] = 255; editor[fg] = 236;
  statln[bg] = 253; statln[fg] = 240;
  lno[bg] = editor[bg]; lno[fg] = 246;
  test[bg] = editor[bg]; test[fg] = COLOR_BLUE;
}

void init_color_pairs(Theme theme) {
  switch (theme) {
    case default_light: set_default_light(); break;
  }
  init_pair(EDITOR_PAIR, editor[fg], editor[bg]);
  init_pair(STATLN_PAIR, statln[fg], statln[bg]);
  init_pair(LNO_PAIR, lno[fg], lno[bg]);
  init_pair(TEST_PAIR, test[fg], test[bg]);
}

