// up and down movements [v]
// 
#include <ncurses.h>
#include <stdint.h>
#include "gap_buffer.c"
#include "include/rust_itypes.h"
#include <stdbool.h>

#define ALLOC_FAC 1024
#define lc_t u16
#define min(a,b) ((a) < (b) ? (a) : (b))

struct span {
  lc_t begin;
  lc_t end;
};

// An "_c" in the following struct names can be abbreviated as _component.
 
// lines component: the idea is to indroduce concept of a line from
// a stream of buffer using '\n' character. I used the following lookup
// table structure. Each line can be indexed this way. 
//  | [line_no] | begin | end |
//  |    [0]    |   0   |  18 |
//  |    [1]    |   19  |  32 |
struct lines_c { 
  struct span *map; // lines lookup table
  lc_t count; // total number of lines
  lc_t capacity; // capacity of *map
};

// cursor component holds x and y position terminal cursor
struct cursor_c { 
  u16 x;
  u16 y;
  lc_t lno; // line number of cursor
}; 

struct view_c {
  usize offset; // points to buffer index where the view should begin
  lc_t lno; // line number of view ptr in the buffer
};

enum modifier {
  lock_cursx = 0x8000,
};

typedef struct {
  u16 modifiers;
  struct cursor_c curs;
  struct view_c view;
  struct lines_c lines;
} Editor;

static inline void editor_set_mods(Editor* ed, u16 mods) { ed->modifiers = ed->modifiers | mods; }
static inline void editor_reset_mods(Editor* ed, u16 mods) { ed->modifiers = ed->modifiers & ( mods ^ 0xffff); }
static inline void editor_modify_cursx(Editor* ed, u16 pos) { if (!(ed->modifiers & lock_cursx)) ed->curs.x = pos; }

Editor editor_init(lc_t lc) {
  Editor ed;
  ed.lines.capacity = lc,
  ed.lines.map = (struct span*)malloc(sizeof(struct span) * lc);
  return ed;
}

void editor_free(Editor* ed) {
  free(ed->lines.map);
  memset(ed, 0, sizeof(Editor));
}

void editor_linefy_buf(Editor* ed, GapBuffer* gap) {
  if (GAPBUF_LEN(gap) == 0) {
    ed->lines.count = ed->view.lno = ed->curs.lno = 0;
    return;
  }
  usize lc = 0;
  bool is_line_start = true; 
  for (usize i = 0; i <= gap->end; i++) {
    if (is_line_start) {
      ed->lines.map[lc].begin = GAPBUF_GET_LOGICAL_INDEX(gap, i);
      is_line_start = false;
    }
    if (i == ed->view.offset) ed->view.lno = lc;
    if (i == gap->c) {
      ed->curs.lno = lc;
      ed->curs.y = lc - ed->view.lno;
      editor_modify_cursx(ed, GAPBUF_GET_LOGICAL_INDEX(gap, i) - ed->lines.map[lc].begin);
      i = (gap->ce < gap->end) ? gap->ce : gap->end;
      continue;
    }
    
    if (gap->start[i] == '\n') {
      ed->lines.map[lc].end = GAPBUF_GET_LOGICAL_INDEX(gap, i);
      is_line_start = true;
      lc++;
      if (lc >= ed->lines.capacity) {
        ed->lines.capacity += ALLOC_FAC;
        ed->lines.map = realloc(ed->lines.map, sizeof(struct span) * ed->lines.capacity);
      }
    }
  }
  ed->lines.count = lc;
  if (!is_line_start) { // if there is no '\n' at the end
    ed->lines.map[lc].end = GAPBUF_LEN(gap) - 1;
    ed->lines.count = lc + 1;
  }
}

#define SCROLL_STEPS 2
void editor_curs_mov_up(Editor* ed, GapBuffer* gap, u16 steps) {
  if (ed->curs.lno == 0) return;
  steps = (steps > ed->curs.lno) ? ed->curs.lno : steps;
  u16 target_lno = ed->curs.lno - steps;
  u16 target_ln_len = ed->lines.map[target_lno].end - ed->lines.map[target_lno].begin;
  u16 target_pos = ed->lines.map[target_lno].begin + min(ed->curs.x, target_ln_len);
  while (gap->c > target_pos) gap_left(gap);
}

void editor_curs_mov_down(Editor* ed, GapBuffer* gap, u16 steps) {
  if (ed->curs.lno == ed->lines.count - 1) return;
  steps = (ed->curs.lno + steps > ed->lines.count - 1) ? ed->lines.count - ed->curs.lno - 1 : steps;
  u16 target_lno = ed->curs.lno + steps;
  u16 target_ln_len = ed->lines.map[target_lno].end - ed->lines.map[target_lno].begin;
  u16 target_pos = ed->lines.map[target_lno].begin + min(ed->curs.x, target_ln_len);
  while (gap->c < target_pos) gap_right(gap);
}

void editor_update_view(Editor* ed, GapBuffer* gap) {
  editor_linefy_buf(ed, gap);
  while (ed->curs.lno - ed->view.lno > LINES - 1) {
    ed->view.offset = ed->lines.map[ed->view.lno + 1].begin;
    editor_linefy_buf(ed, gap);
  }
  while (ed->curs.lno - ed->view.lno < 0) {
    ed->view.offset = ed->lines.map[ed->view.lno - 1].begin;
    editor_linefy_buf(ed, gap);
  }
}

void editor_draw(Editor* ed, GapBuffer* gap) {
  struct span* ln_map = ed->lines.map;
  erase();
  for (usize ln = 0; ln < LINES && ln < ed->lines.count; ln++) {
    for (usize i = ln_map[ln + ed->view.lno].begin; i <= ln_map[ln + ed->view.lno].end; i++) {
      addch(gap_getch(gap, i));
    }
  }
  u16 x = gap->c - ed->lines.map[ed->curs.lno].begin;
  move(ed->curs.y, x);
}

void editor_handle_mods(Editor* ed, u32 ch) {
  switch (ch) {
    case KEY_UP:
    case KEY_DOWN: editor_set_mods(ed, lock_cursx);
      break;
    default: editor_reset_mods(ed, lock_cursx);
      break;
  }
}

i32 main() {
  Editor ed = editor_init(ALLOC_FAC);
  GapBuffer gap = gap_init(ALLOC_FAC);

  initscr();
  noecho();
  curs_set(1);
  keypad(stdscr, TRUE);
  cbreak();

  u32 ch;
  while ((ch = getch()) != 27) {
    switch (ch) {
      case KEY_LEFT:
        gap_left(&gap);
        break;
      case KEY_RIGHT:
        gap_right(&gap);
        break;
      case KEY_UP:
        editor_curs_mov_up(&ed, &gap, SCROLL_STEPS);
        break;
      case KEY_DOWN:
        editor_curs_mov_down(&ed, &gap, SCROLL_STEPS);
        break;
      case KEY_BACKSPACE:
        gap_removec(&gap);
        break;
      case KEY_ENTER:
        gap_insertc(&gap, '\n');
        break;
      default:
        gap_insertc(&gap, ch);
        break;
    }
    editor_handle_mods(&ed, ch);
    editor_update_view(&ed, &gap);
    editor_draw(&ed, &gap);
    refresh();
  }
  
  endwin();
  editor_free(&ed);
  gap_free(&gap);
  return 0;
}
