/*
TODO
 - up and down movements [v]
 - snap scrolling [v]
 - split editor operations into component level [v]
 - make editior updation incremental [v]
 - handle horizontal text overflows [_]
 - make Editor window attachable [_]
*/

#include <ncurses.h>
#include <stdint.h>
#include "gap_buffer.c"
#include "include/rust_itypes.h"
#include <stdbool.h>

// realloc step for lines.map table
#define ALLOC_STEP 1024

// lc_t will specify the maximum number of lines possible. 65000 is enough for me
#define lc_t u16
#define min(a,b) ((a) < (b) ? (a) : (b))

// for view allignment
#define SCROLL_MARGIN 3

// An "_c" in the following struct names can be abbreviated as _component.
 
// A line is an abstraction over the stream of text. Enables performing
// operations on editor at line level. line map is like an associative array.
// array index is line number, and its content is start position of line
// inside the buffer.
struct lines_c { 
  u32 *map; // lines lookup table
  lc_t count; // total number of lines
  lc_t capacity; // capacity of *map
};

// cursor component holds logical x and y position of cursor in the buffer.
struct cursor_c { 
  u16 x;
  u16 y;
}; 

// view component marks the begin position of text drawing, as well
// as it makes scrolling possible.
struct view_c {
  u32 offset; // logical index of buffer where the view should begin
  u32 x;
  lc_t y;
};

typedef struct {
  u16 mods; // stores all modifiers used in the editor.
  // states should only be modified through the designated methods.
  struct cursor_c curs;
  struct view_c view;
  struct lines_c lines;
} Editor;

// all the modifier types are defined here
enum modifiers {
  _lock_cursx = 0x8000, // prevent writing to editor.curs.x; method: editor_update_cursx()
};

// used to set modifier. you can input multiple modifier using the OR operator
static inline void editor_set_mods(Editor* ed, u16 new_mods) { ed->mods = ed->mods | new_mods; }

// used to reset mods. you can input multiple mods using the OR operator
static inline void editor_reset_mods(Editor* ed, u16 new_mods) { ed->mods = ed->mods & ( new_mods ^ 0xffff); }

// used to check modifiers
static inline bool editor_has_mods(Editor* ed, u16 mods) { return (ed->mods & mods) == mods; }

// updates editor.curs.x to pos if the lock_cursx state is set to zero.
static inline void editor_update_cursx(Editor* ed, u16 pos) { if (!editor_has_mods(ed, _lock_cursx)) ed->curs.x = pos; }

// initializes Editor entity.
Editor editor_init(lc_t lc) {
  Editor ed = {0};
  ed.lines.capacity = lc;
  ed.lines.count = 1;
  ed.lines.map = (u32*)malloc(sizeof(u32) * lc);
  if (!ed.lines.map) {
    perror("failed to do malloc for lines.map");
    exit(-1);
  }
  memset(ed.lines.map, 0, sizeof(u32) * lc);
  return ed;
}

void editor_free(Editor* ed) {
  free(ed->lines.map);
  memset(ed, 0, sizeof(Editor));
}

#define LN_EMPTY U32_MAX
// returns end position of give lno from line componet of editor.
// this is part of the line component design choice.
u32 editor_lnend(Editor* ed, GapBuffer* gap, lc_t lno) {
  u32 i = ed->lines.map[lno];
  u32 len = GAPBUF_LEN(gap);
  if (i >= len) return LN_EMPTY;
  while (i < len - 1 && gap_getch(gap, i) != '\n') i++;
  return i;
}

// expand lines.map, allocates more memory for ln.map if needed.
void lines_increment_count(struct lines_c* ln) {
  if (ln->count >= ln->capacity) {
    ln->capacity += ALLOC_STEP;
    ln->map = realloc(ln->map, sizeof(u32) * ln->capacity);
    if (!ln->map) {
      perror("failed to do realloc for lines.map");
      exit(-1);
    }
  }
  ln->count++;
}

// updates the view component of editor, expects an updated curs component
void editor_update_view(Editor* ed, GapBuffer* gap) {
  while (ed->curs.y - ed->view.y > LINES - 1 - SCROLL_MARGIN) { // down scrolling
    ed->view.offset = ed->lines.map[++ed->view.y];
  }
  while (ed->view.y > 0 && ed->curs.y < ed->view.y + SCROLL_MARGIN) { // upwards
    ed->view.offset = ed->lines.map[--ed->view.y];
  }
}

// renders text on screen. expects updated lines, cursor and view
void editor_draw(Editor* ed, GapBuffer* gap) {
  erase();
  for (u32 ln = 0; ln < LINES  && ln + ed->view.y < ed->lines.count; ln++) {
    u32 lnend = editor_lnend(ed, gap, ln + ed->view.y);
    if (lnend != LN_EMPTY) {
      for (u32 i = ed->lines.map[ln + ed->view.y]; i <= lnend; i++) {
        addch(gap_getch(gap, i));
      }
    }
  }
  move(ed->curs.y - ed->view.y, gap->c - ed->lines.map[ed->curs.y]);
}

// to handle implicit editor modifiers like preserving cursx position
// when doing a vertical scroll.
void editor_handle_implicit_mods(Editor* ed, u32 ch) {
  switch (ch) {
    case KEY_UP:
    case KEY_DOWN: editor_set_mods(ed, _lock_cursx); return;
  }
  editor_reset_mods(ed, _lock_cursx);
}

// increments all positions after the line where cursor is. expects a non updated cursor
static inline void editor_increment_next_lines(Editor* ed) {
  for (lc_t i = ed->curs.y + 1; i < ed->lines.count; i++) ed->lines.map[i]++;
}

// decrements all positions after the line where cursor is. expects a non updated cursor
static inline void editor_decrement_next_lines(Editor* ed) {
  for (lc_t i = ed->curs.y + 1; i < ed->lines.count; i++) ed->lines.map[i]--;
}

// makes a new line entry in the lines.map at curs.y + 1 position. expect a non updated cursor component.
void editor_add_nl(Editor* ed, u32 logical_pos) {
  lines_increment_count(&ed->lines);
  for (lc_t i = ed->lines.count - 1; i > ed->curs.y + 1; i--) {
    ed->lines.map[i] = ed->lines.map[i - 1]; // shifting right from curs_lno + 1 pos
  }
  // curs_lno + 1 position should be unoccupied now
  ed->lines.map[ed->curs.y + 1] = logical_pos; // new position is inserted there
}

// remove a line entry from lines.map. expect non updated cursor in editor.
void editor_remove_nl(Editor* ed) {
  for (lc_t i = ed->curs.y + 1; i < ed->lines.count; i++) {
    ed->lines.map[i - 1] = ed->lines.map[i];
  }
  ed->lines.count--;
}

// inserts ch into gap buffer, updates cursor and lines.map.
void editor_insertch(Editor* ed, GapBuffer* gap, u32 ch) {
  gap_insertch(gap, ch);
  editor_increment_next_lines(ed);
  if (ch == '\n') {
    editor_add_nl(ed, gap->c);
    ed->curs.y++;
  }
  editor_update_cursx(ed, gap->c - ed->lines.map[ed->curs.y]);
}

// removes a character from gap buffer, updates it on cursor and lines.map
void editor_removech(Editor* ed, GapBuffer* gap) {
  u8 removing_ch = gap_getch(gap, gap->c - 1);
  if (!removing_ch) return;
  editor_decrement_next_lines(ed);
  gap_removech(gap);
  if (removing_ch == '\n') {
    editor_remove_nl(ed);
    ed->curs.y--;
  }
  editor_update_cursx(ed, gap->c - ed->lines.map[ed->curs.y]);
}

// handler for left cursor movement.
void editor_curs_mov_left(Editor* ed, GapBuffer* gap) {
  if (gap->c > 0) {
    u8 prev_ch = gap->start[gap->c - 1];
    if (prev_ch == '\n') {
      ed->curs.y--;
    }
    gap_left(gap);
    editor_update_cursx(ed, gap->c - ed->lines.map[ed->curs.y]);
  }
}

// handler for cursor movement towards right
void editor_curs_mov_right(Editor* ed, GapBuffer* gap) {
  u8 next_ch = gap_getch(gap, gap->c);
  if (next_ch != 0) {
    if (next_ch == '\n') ed->curs.y++;
    gap_right(gap);
    editor_update_cursx(ed, gap->c - ed->lines.map[ed->curs.y]);
  }
}

// moves the cursor vertically up by `steps` times. The lock_cursx state
// should be set before calling this method inorder to preserve curs.x
void editor_curs_mov_up(Editor* ed, GapBuffer* gap, u16 steps) {
  if (ed->curs.y == 0) return;
  steps = (steps > ed->curs.y) ? ed->curs.y : steps;
  u16 target_lno = ed->curs.y - steps;
  u16 target_ln_len = editor_lnend(ed,gap, target_lno) - ed->lines.map[target_lno];
  u16 target_pos = ed->lines.map[target_lno] + min(ed->curs.x, target_ln_len);
  while (gap->c > target_pos) gap_left(gap);
  ed->curs.y -= steps;
}

// // moves cursor down
void editor_curs_mov_down(Editor* ed, GapBuffer* gap, u16 steps) {
  if (ed->curs.y == ed->lines.count - 1) return;
  steps = (ed->curs.y + steps > ed->lines.count - 1) ? ed->lines.count - ed->curs.y - 1 : steps;
  u16 target_lno = ed->curs.y + steps;
  u16 target_ln_len = editor_lnend(ed, gap, target_lno) - ed->lines.map[target_lno];
  u16 target_pos = ed->lines.map[target_lno] + min(ed->curs.x, target_ln_len);
  while (gap->c < target_pos) gap_right(gap);
  ed->curs.y++;
}

i32 main() {
  Editor ed = editor_init(ALLOC_STEP);
  GapBuffer gap = gap_init(ALLOC_STEP);

  initscr();
  noecho();
  curs_set(1);
  keypad(stdscr, TRUE);
  cbreak();

  u32 ch;
  while ((ch = getch()) != 27) {
    editor_handle_implicit_mods(&ed, ch); // states should be set before doing anything.
    switch (ch) {
      case KEY_LEFT: editor_curs_mov_left(&ed, &gap); break;
      case KEY_RIGHT: editor_curs_mov_right(&ed, &gap); break;
      case KEY_UP: editor_curs_mov_up(&ed, &gap, 1); break;
      case KEY_DOWN: editor_curs_mov_down(&ed, &gap, 1); break;
      case KEY_BACKSPACE: editor_removech(&ed, &gap); break;
      case KEY_ENTER: editor_insertch(&ed, &gap, '\n'); break;
      default: editor_insertch(&ed, &gap, ch); break;
    }
    editor_update_view(&ed, &gap);
    editor_draw(&ed, &gap);
    refresh();
  }
  
  endwin();
  editor_free(&ed);
  gap_free(&gap);
  return 0;
}
