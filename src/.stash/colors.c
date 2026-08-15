#pragma once
#include "include/itypes.h"
#include <ncurses.h>

enum {
  EDITOR_PAIR = 1,
  STATLN_PAIR,
  STATLN_WARN_PAIR,
  COMMENT_PAIR,
  CURS_PAIR,
  TXT_GREEN,
};

#define bg 0
#define fg 1

i16 editor[2],
statln[2],
comment[2],
statln_warn[2],
txt_green[2],
curs[2];

typedef enum {
  default_light,
} Theme;

static void set_default_light() {
  editor[bg] = 255; editor[fg] = 236;
  statln[bg] = 253; statln[fg] = 240;
  comment[bg] = editor[bg]; comment[fg] = 248;
  statln_warn[bg] = statln[bg]; statln_warn[fg] = COLOR_RED;
  curs[bg] = 34; curs[fg] = editor[bg];
  txt_green[bg] = editor[bg]; txt_green[fg] = 34;
}

void set_theme(Theme theme) {
  switch (theme) {
    case default_light: set_default_light(); break;
  }
  init_pair(EDITOR_PAIR, editor[fg], editor[bg]);
  init_pair(STATLN_PAIR, statln[fg], statln[bg]);
  init_pair(COMMENT_PAIR, comment[fg], comment[bg]);
  init_pair(STATLN_WARN_PAIR, statln_warn[fg], statln_warn[bg]);
  init_pair(CURS_PAIR, curs[fg], curs[bg]);
  init_pair(TXT_GREEN, txt_green[fg], txt_green[bg]);
}

