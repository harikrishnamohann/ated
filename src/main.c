#include <ncurses.h>
#include "include/itypes.h"
#include "editor.c"

Editor* ed = NULL;
WINDOW* edwin = NULL;

void cleanup() {
  if (ed != NULL)
    editor_free(&ed);

  if (edwin != NULL)
    delwin(edwin);

  endwin();
}

i32 main(i32 argc, char** argv) {
  atexit(cleanup);
  initscr();
  noecho();
  curs_set(1);
  keypad(stdscr, TRUE);
  cbreak();

  edwin = newwin(LINES, COLS, 0, 0);
  ed = editor_init(argv[1]);
  keypad(edwin, TRUE);

  u32 ch;
  editor_draw(edwin, ed);
  wrefresh(edwin);
  while ((ch = wgetch(edwin)) != CTRL('q')) {
    if (ch != ERR) {
      if (ch >= 32 && ch < 127) { // ascii printable character range
        editor_insert(ed, ch);
      } else {
        switch (ch) {
          case KEY_LEFT: curs_mov_left(ed, 1); break;
          case KEY_RIGHT: curs_mov_right(ed, 1); break;
          case KEY_UP: curs_mov_up(ed, 1); break;
          case KEY_DOWN: curs_mov_down(ed, 1); break;
          case KEY_BACKSPACE: editor_removel(ed); break;
          case KEY_DC: editor_remover(ed); break;
          case '\n': editor_insert_newline(ed); break;
          case '\t': editor_insert(ed, '\t'); break;
          case CTRL('u'): editor_undo(ed); break;
          case CTRL('r'): editor_redo(ed) ;break;
          case CTRL('s'): editor_write_file(ed); break;
        }
      }
      editor_draw(edwin, ed);
    }
    wrefresh(edwin);
  }

  return 0;
}
