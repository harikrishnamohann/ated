#include <ncurses.h>
#include <stdint.h>
#include "gap_buffer.c"

// up and down movements [v]

typedef struct {
  struct {
    uint8_t x;
    uint8_t y;
  } curs; // holds x and y position of visual cursor in the terminal
  uint32_t view; // view pointer
  uint16_t view_lno; // line number of view ptr in the buffer
  uint16_t curs_lno; // line number of cursor
  struct {
    uint16_t *ln_refs;
    uint16_t count;
    uint16_t capacity;
  } lines;
} Editor; 

Editor editor_init(uint16_t lc) {
   Editor ed = {
    .curs = {0},
    .view_lno = 1,
    .curs_lno = 1,
    .lines = {
      .ln_refs = malloc(sizeof(uint16_t) * lc),
      .capacity = lc,
    },
  };
  memset(ed.lines.ln_refs, 0x0, sizeof(uint16_t) * lc);
  return ed;
}

void editor_free(Editor* ed) {
  free(ed->lines.ln_refs);
  memset(ed, 0, sizeof(Editor));
}

void editor_update_lines(Editor* ed, GapBuffer* gap) {
  uint32_t lc = 0, len = GAP_CH_COUNT(gap); 
  if (len == 0) {
    ed->view_lno = ed->curs_lno = ed->lines.count = lc;
    return;
  }
  for (uint32_t i = 0; i < len; i++) {
    if (i == ed->view) ed->view_lno = lc;
    if (i + 1 == gap->c) ed->curs_lno = lc;
    if (gap_getch(gap, i) == '\n') {
      ed->lines.ln_refs[lc++] = i; // review this later
    }
  }
  ed->lines.count = lc;
}

// todo
void editor_fetch_view(Editor* ed, GapBuffer* gap) {
  editor_update_lines(ed, gap);
  // while (ed->curs_lno < ed->view_lno) {
  //   editor_update_lines(ed, gap);
  // }

  int i = ed->view_lno;
  while (ed->curs_lno - ed->view_lno > LINES - 1) {
    ed->view = ed->lines.ln_refs[++i];
    editor_update_lines(ed, gap);
  }
}

void editor_draw(Editor* ed, GapBuffer* gap) {
  uint32_t x = 0, y = 0, len = GAP_CH_COUNT(gap);
  ed->curs.x = ed->curs.y = 0;
  erase();
  for (int32_t i = ed->view; i < len && y < LINES; i++) {
    uint8_t ch = gap_getch(gap, i);
    mvaddch(y, x++, ch);
    if (ch == '\n') {
      y++;
      x = 0;
    }
    if (i + 1 == gap->c) {
      ed->curs.x = x;
      ed->curs.y = y;
    }
  }
  move(ed->curs.y, ed->curs.x);
}

int main() {
  Editor ed = editor_init(1024);
  GapBuffer gap = gap_init(1024);

  initscr();
  noecho();
  curs_set(1);
  keypad(stdscr, TRUE);
  cbreak();

  uint32_t ch;
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
        gap_removec(&gap);
        break;
      case 10: case KEY_ENTER:
        gap_insertc(&gap, '\n');
        break;
      default:
        gap_insertc(&gap, ch);
        break;
    }
    editor_fetch_view(&ed, &gap);
    editor_draw(&ed, &gap);
    refresh();
  }
  
  endwin();
  editor_free(&ed);
  gap_free(&gap);
  return 0;
}
