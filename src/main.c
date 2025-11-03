#include <stdbool.h>
#include <ncurses.h>
#include "include/ated.h"
#include "include/itypes.h"
#include "editor.c"

i32 main() {
  initscr();
  noecho();
  curs_set(1);
  keypad(stdscr, TRUE);
  cbreak();

  #define H 20
  #define W 50
  WINDOW* edwin = newwin(H, W, CENTER(LINES, H), CENTER(COLS, W));

  editor_process(edwin);

  delwin(edwin);
  endwin();
  return 0;
}
