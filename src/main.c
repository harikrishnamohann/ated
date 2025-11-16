#include <ncurses.h>
#include "include/itypes.h"
#include "editor.c"

i32 main() {
  initscr();
  noecho();
  curs_set(1);
  keypad(stdscr, TRUE);
  cbreak();

  WINDOW* edwin = newwin(LINES, COLS, 0, 0);

  editor_process(edwin);

  delwin(edwin);
  endwin();
  return 0;
}
