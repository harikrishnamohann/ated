// todo
// up and down movements [v]
// horizontal text overflows [_]

#include <ncurses.h>
#include <stdint.h>
#include "gap_buffer.c"
#include "include/rust_itypes.h"
#include <stdbool.h>

// realloc step for lines.map table
#define ALLOC_FAC 1024

// lc_t will specify the maximum number of lines possible. 65000 is enough for me
#define lc_t u16
#define min(a,b) ((a) < (b) ? (a) : (b))

// for view allignment
#define LINE_MARGIN 1

// for storing line mapping
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

typedef struct {
  u16 state; // stores all the states used in the editor.
  // states should only be modified through the designated methods.
  struct cursor_c curs;
  struct view_c view;
  struct lines_c lines;
} Editor;

// all the modifier types are defined here
enum states {
  lock_cursx = 0x8000, // prevent writing to editor.curs.x; method: editor_update_cursx()
};

// used to set states. you can input multiple states using the OR operator
static inline void editor_set_states(Editor* ed, u16 states) { ed->state = ed->state | states; }

// used to reset states. you can input multiple states using the OR operator
static inline void editor_reset_states(Editor* ed, u16 states) { ed->state = ed->state & ( states ^ 0xffff); }

// used to check states
// static inline bool editor_has_states(Editor* ed, u16 states) { return ed->state & states; }

// updates editor.curs.x to pos if the lock_cursx state is set to zero.
static inline void editor_update_cursx(Editor* ed, u16 pos) { if (!(ed->state & lock_cursx)) ed->curs.x = pos; }

// initializes Editor entity.
Editor editor_init(lc_t lc) {
  Editor ed;
  ed.lines.capacity = lc,
  ed.lines.map = (struct span*)malloc(sizeof(struct span) * lc);
  if (!ed.lines.map) {
    perror("failed to do malloc for lines.map");
    exit(-1);
  }
  return ed;
}

void editor_free(Editor* ed) {
  free(ed->lines.map);
  memset(ed, 0, sizeof(Editor));
}

// moves the cursor vertically up by `steps` times. The lock_cursx state
// should be set before calling this method inorder to preserve cursor position
// in x direction.
void editor_curs_mov_up(Editor* ed, GapBuffer* gap, u16 steps) {
  if (ed->curs.lno == 0) return;
  steps = (steps > ed->curs.lno) ? ed->curs.lno : steps;
  u16 target_lno = ed->curs.lno - steps;
  u16 target_ln_len = ed->lines.map[target_lno].end - ed->lines.map[target_lno].begin;
  u16 target_pos = ed->lines.map[target_lno].begin + min(ed->curs.x, target_ln_len);
  while (gap->c > target_pos) gap_left(gap);
}

// moves the cursor vertically down by `steps` times. The lock_cursx state
// should be set before calling this method inorder to preserve cursor position
// in x direction.
void editor_curs_mov_down(Editor* ed, GapBuffer* gap, u16 steps) {
  if (ed->curs.lno == ed->lines.count - 1) return;
  steps = (ed->curs.lno + steps > ed->lines.count - 1) ? ed->lines.count - ed->curs.lno - 1 : steps;
  u16 target_lno = ed->curs.lno + steps;
  u16 target_ln_len = ed->lines.map[target_lno].end - ed->lines.map[target_lno].begin;
  u16 target_pos = ed->lines.map[target_lno].begin + min(ed->curs.x, target_ln_len);
  while (gap->c < target_pos) gap_right(gap);
}

// to handle implicit editor states like preserving cursx position
// when doing a vertical scroll.
void editor_handle_states(Editor* ed, u32 ch) {
  switch (ch) {
    case KEY_UP:
    case KEY_DOWN: editor_set_states(ed, lock_cursx);
      break;
    default: editor_reset_states(ed, lock_cursx);
      break;
  }
}

// this method will gracefully abstract the GapBuffer into lines and saves
// it in the line component of editor.
void editor_linefy_buf(Editor* ed, GapBuffer* gap) {
  if (GAPBUF_LEN(gap) == 0) { // if the gap_buffer is empty
    ed->lines.count = ed->view.lno = ed->curs.lno = 0;
    return;
  }
  usize lc = 0; // used to track Line Count. incremented on each '\n' character.
  bool is_line_start = true; // a state for recording start and end positions of a line. 
  for (usize i = 0; i <= gap->end; i++) {
    if (is_line_start) { // marks current logical index as begining of current line.
      ed->lines.map[lc].begin = GAPBUF_GET_LOGICAL_INDEX(gap, i);
      is_line_start = false;
    }
    if (i == ed->view.offset) ed->view.lno = lc;
    if (i == gap->c) { // we have to skip the gap after saving cursor line number.
      ed->curs.lno = lc;
      ed->curs.y = lc - ed->view.lno;
      // conditionally updates curs.x as offset from beginning of current line.
      editor_update_cursx(ed, GAPBUF_GET_LOGICAL_INDEX(gap, i) - ed->lines.map[lc].begin);
      i = (gap->ce < gap->end) ? gap->ce : gap->end;
      continue;
    }
    
    if (gap->start[i] == '\n') {
      ed->lines.map[lc].end = GAPBUF_GET_LOGICAL_INDEX(gap, i); // saves end position
      is_line_start = true; // to mark next character as start of next line.
      lc++;
      if (lc >= ed->lines.capacity) { // handle line map overflows.
        ed->lines.capacity += ALLOC_FAC;
        ed->lines.map = realloc(ed->lines.map, sizeof(struct span) * ed->lines.capacity);
        if (!ed->lines.map) {
          perror("failed to do realloc for lines.map");
          exit(-1);
        }
      }
    }
  }
  ed->lines.count = lc;
  if (!is_line_start) { // if there is no '\n' at the end
    ed->lines.map[lc].end = GAPBUF_LEN(gap) - 1;
    ed->lines.count = lc + 1;
  }
}

// updates the view component of editor
void editor_update_view(Editor* ed, GapBuffer* gap) {
  while (ed->curs.lno - ed->view.lno > LINES - 1) {
    ed->view.offset = ed->lines.map[ed->view.lno + 1].begin;
    editor_linefy_buf(ed, gap);
  }
  while (ed->curs.lno - ed->view.lno < 0) {
    ed->view.offset = ed->lines.map[ed->view.lno - 1].begin;
    editor_linefy_buf(ed, gap);
  }
}

// renders the gap buffer as desired.
void editor_draw(Editor* ed, GapBuffer* gap) {
  struct span* ln_map = ed->lines.map;
  erase();
  for (usize ln = 0; ln < LINES  && ln < ed->lines.count; ln++) {
    for (usize i = ln_map[ln + ed->view.lno].begin; i <= ln_map[ln + ed->view.lno].end; i++) {
      addch(gap_getch(gap, i));
    }
  }
  move(ed->curs.y, gap->c - ed->lines.map[ed->curs.lno].begin);
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
    editor_handle_states(&ed, ch); // states should be set before doing anything.
    switch (ch) {
      case KEY_LEFT:
        gap_left(&gap);
        break;
      case KEY_RIGHT:
        gap_right(&gap);
        break;
      case KEY_UP:
        editor_curs_mov_up(&ed, &gap, 1);
        break;
      case KEY_DOWN:
        editor_curs_mov_down(&ed, &gap, 1);
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
    editor_linefy_buf(&ed, &gap);
    editor_update_view(&ed, &gap);
    editor_draw(&ed, &gap);
    refresh();
  }
  
  endwin();
  editor_free(&ed);
  gap_free(&gap);
  return 0;
}
