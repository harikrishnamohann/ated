#include <stdlib.h>
#include <locale.h>

#define _XOPEN_SOURCE_EXTENDED
#include <ncursesw/ncurses.h>

#include "include/itypes.h"
#include "colors.c"
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
  setlocale(LC_ALL, "");
  atexit(cleanup);
  initscr();
  noecho();
  curs_set(0);
  keypad(stdscr, TRUE);
  cbreak();
  mousemask(BUTTON1_PRESSED |
            BUTTON1_RELEASED |
            REPORT_MOUSE_POSITION |
            BUTTON4_PRESSED |
            BUTTON5_PRESSED |
            BUTTON_SHIFT, NULL);

  edwin = newwin(LINES, COLS, 0, 0);
  ed = editor_init(argv[1]);
  keypad(edwin, TRUE);

  if (has_colors()) {
    start_color();
    set_theme(default_light);
    wbkgd(edwin, COLOR_PAIR(EDITOR_PAIR) | ' ');
  }

  if (!has_ic()) {
      printf("Your terminal does not support insert character.\n");
      exit(EXIT_FAILURE);
  }

  MEVENT mevnt;

  editor_draw(edwin, ed);
  wrefresh(edwin);
  u32 ch;
  do {
    wget_wch(edwin, &ch);
    if (ch == ERR) {
      continue;
    } else if (ch == KEY_MOUSE) {
      if (getmouse(&mevnt) == OK) {
        if (mevnt.bstate & BUTTON1_PRESSED) {
          // TODO
        } else if (mevnt.bstate & BUTTON1_RELEASED) {
          // TODO
        } else if (mevnt.bstate & BUTTON4_PRESSED) {
          if (mevnt.bstate & BUTTON_SHIFT) {
            curs_mov_left(ed, 3);            
          } else {
            curs_mov_up(ed, 3);
          }
        } else if (mevnt.bstate & BUTTON5_PRESSED) {
          if (mevnt.bstate & BUTTON_SHIFT) {
            curs_mov_right(ed, 3);
          } else {
            curs_mov_down(ed, 3);
          }
        }
      }      
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
        case CTRL('s'): write_to_file(ed); break;
        case CTRL('q'): editor_exit(ed); break;
        default:
          if (ch >= 32)
            editor_insert(ed, ch);
          break;
      }
    }
    editor_draw(edwin, ed);
    wrefresh(edwin);
  } while (1);
  exit(EXIT_SUCCESS);
}

