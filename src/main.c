#include <ncurses.h>
#include <stdint.h>
#include "gap_buffer.c"

// handle overflows(horizontal and vertical)
// up and down movements

struct cursor;

struct cursor {
  uint32_t lc;
  uint32_t curs;
  uint32_t view;
};

struct cursor linefy(const struct gap_buf* gap) {
  char* ptr = gap->start;  
  struct cursor linfo = {1};
  while (ptr < gap->c) {
    if (ptr == gap->view) {
      linfo.view = linfo.lc;
    }
    if (*ptr == '\n') {
      linfo.lc++;
    }
    ptr++;
  }
  linfo.curs = linfo.lc;
  if (gap->ce != gap->end) {
    ptr = gap->ce + 1;
    while (ptr <= gap->end) {
      if (*ptr == '\n') {
        linfo.lc++;
      }
      ptr++;
    }
  }
  return linfo;
}

void view_advance(struct gap_buf* gap) {
  while (gap->view < gap->c && *gap->view != '\n') gap->view++;
  gap->view++;
}

void view_descent(struct gap_buf* gap) {
  if (*gap->view == '\n') gap->view--;
  while (gap->view > gap->start && *(gap->view - 1) != '\n') gap->view--;
}

void draw_text(struct gap_buf* gap) {
  struct cursor line = linefy(gap);
  while (line.curs < line.view) { // i don't understand what's happening here.
    view_descent(gap);
    line = linefy(gap);
  }
  while (line.curs - line.view >= LINES - 1) {
    view_advance(gap);
    line = linefy(gap);
  }

  erase();
  uint32_t curs_x = 0, curs_y = 0, x = 0, y = 0;
  char* cursor = gap->view;
  while (cursor <= gap->end && y < LINES - 1) {
    if (cursor == gap->c) {
      curs_x = x;
      curs_y = y;
      if (gap->ce == gap->end) break; 
      cursor = gap->ce + 1;
    }
    mvaddch(y, x++, *cursor);
    if (*cursor == '\n') {
      y++;
      x = 0;
    }
    cursor++;
  }
  mvprintw(LINES - 1, 1, "v:%ld c:%ld lc:%d", gap->view - gap->start, gap->c - gap->start, line.lc);
  move(curs_y, curs_x);
}

int main() {
  struct gap_buf gap = gap_init(1);
  initscr();
  noecho();
  curs_set(1);
  keypad(stdscr, TRUE);
  cbreak();
  
  refresh();
  
  int ch;
  while ((ch = getch()) != 27) {
    switch (ch) {
      case KEY_LEFT:
        gap_left(&gap, 1);
        break;
      case KEY_RIGHT:
        gap_right(&gap, 1);
        break;
      case KEY_UP:
        break;
      case KEY_DOWN:
        break;
      case KEY_BACKSPACE:
        gap_remove_ch(&gap);
        break;
      case KEY_ENTER:
        gap_insert_ch(&gap, '\n');
        break;
      default:
        gap_insert_ch(&gap, (char)ch);
        break;
    }
    draw_text(&gap);
    refresh();
  }
  
  endwin();
  gap_free(&gap);
  return 0;
}
