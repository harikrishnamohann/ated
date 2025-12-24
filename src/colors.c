#pragma once
#include "include/itypes.h"
#include <ncurses.h>

enum {
  EDITOR_PAIR = 1,
  STATLN_PAIR,
  STATLN_WARN_PAIR,
  ANNOTATE_PAIR,
  TEST_PAIR,
};

#define bg 0
#define fg 1

i16 editor[2],
statln[2],
annotate[2],
statln_warn[2],
test[2];

typedef enum {
  default_light,
} Theme;

static void set_default_light() {
  editor[bg] = 255; editor[fg] = 236;
  statln[bg] = 253; statln[fg] = 240;
  annotate[bg] = editor[bg]; annotate[fg] = 246;
  statln_warn[bg] = statln[bg]; statln_warn[fg] = COLOR_RED;
  test[bg] = editor[bg]; test[fg] = COLOR_BLUE;
}

void set_theme(Theme theme) {
  switch (theme) {
    case default_light: set_default_light(); break;
  }
  init_pair(EDITOR_PAIR, editor[fg], editor[bg]);
  init_pair(STATLN_PAIR, statln[fg], statln[bg]);
  init_pair(ANNOTATE_PAIR, annotate[fg], annotate[bg]);
  init_pair(STATLN_WARN_PAIR, statln_warn[fg], statln_warn[bg]);
  init_pair(TEST_PAIR, test[fg], test[bg]);
}

