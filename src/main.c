#include <ncurses.h>
#include <stdint.h>
#include "gap_buffer.c"

// handle overflows(horizontal and vertical)
// up and down movements

struct editor {
  struct gap_buf gap_buf; // the gap buffer data structure
  struct curs {
    uint32_t x;
    uint32_t y;
  } curs; // holds x and y position of visual cursor in the terminal
  uint32_t view_lno; // line number of view ptr in the buffer
  uint32_t curs_lno; // line number of cursor
  uint32_t lc; // total line count
}; 

#define editor_init(gap_size) \
  (struct editor) { \
    .gap_buf = gap_init(gap_size), \
    .curs = {0}, \
    .view_lno = 1, \
    .curs_lno = 1, \
    .lc = 1, \
  } 

void editor_free(struct editor* editor) {
  gap_free(&(editor->gap_buf));
  editor->curs.x = 0;
  editor->curs.y = 0;
  editor->view_lno = 0;
  editor->curs_lno = 0;
}

void editor_update_lines(struct editor* editor) {
  uint32_t count = 1; 
  struct gap_buf* gap = &editor->gap_buf;
  char* ptr = gap->start;
  while (ptr <= gap->end) {
    if (ptr == gap->view) {
      editor->view_lno = count;
    }
    if (ptr == gap->c) {
      editor->curs_lno = count;
      if (gap->ce != gap->end) {
        ptr = gap->ce + 1;
      } else {
        ptr = gap->ce;
      }
    }
    if (*ptr == '\n') {
      ++count;
    }
    ptr++;
  }
  editor->lc = count;
}

void gap_view_align(struct editor* editor) {
  struct gap_buf* gap = &editor->gap_buf;

  while (editor->curs_lno < editor->view_lno) {
    if (*(gap->view - 1) == '\n') gap->view--;
    while (gap->view > gap->start && *(gap->view - 1) != '\n') gap->view--;
    editor_update_lines(editor);
  }

  while (editor->curs_lno - editor->view_lno > LINES - 1) {
    while (gap->view < gap->c && *gap->view != '\n') gap->view++;
    if (*gap->view == '\n') gap->view++;
    editor_update_lines(editor);
  }
}

void editor_draw(struct editor* editor) {
  gap_view_align(editor);
  erase();
  uint32_t x = 0, y = 0;
  struct gap_buf* gap = &editor->gap_buf;
  char* cursor = gap->view;
  while (cursor <= gap->end && y < LINES) {
    if (cursor == gap->c) {
      editor->curs.x = x;
      editor->curs.y = y;
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
  move(editor->curs.y, editor->curs.x);
}

int main() {
  struct editor editor = editor_init(512);
  initscr();
  noecho();
  curs_set(1);
  keypad(stdscr, TRUE);
  cbreak();
   int ch;
  while ((ch = getch()) != 27) {
    switch (ch) {
      case KEY_LEFT:
        gap_left(&editor.gap_buf, 1);
        break;
      case KEY_RIGHT:
        gap_right(&editor.gap_buf, 1);
        break;
      case KEY_UP:
        break;
      case KEY_DOWN:
        break;
      case KEY_BACKSPACE:
        gap_remove_ch(&editor.gap_buf);
        break;
      case KEY_ENTER:
        gap_insert_ch(&editor.gap_buf, '\n');
        break;
      default:
        gap_insert_ch(&editor.gap_buf, (char)ch);
        break;
    }
    editor_update_lines(&editor);
    editor_draw(&editor);
    refresh();
  }
  
  endwin();
  editor_free(&editor);
  return 0;
}
