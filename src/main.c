#include <stdbool.h>
#include <ncurses.h>
#include "include/ated.h"
#include "include/rust_itypes.h"
#include "editor.c"

i32 main() {
  Editor ed = editor_init(ALLOC_STEP);

  initscr();
  noecho();
  curs_set(1);
  keypad(stdscr, TRUE);
  cbreak();

  #define H 20
  #define W 40
  WINDOW* edwin = newwin(H, W, CENTER(LINES, H), CENTER(COLS, W));
  keypad(edwin, TRUE);

  editor_process(&ed, edwin);

  delwin(edwin);
  endwin();
  editor_free(&ed);
  return 0;
}
