/*
TODO
 - up and down movements [v]
 - snap scrolling [v]
 - split editor operations into component level [v]
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
  _lock_cursx = 0x1000, // prevent writing to editor.curs.x; method: editor_update_cursx()
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
  ed.lines.capacity = lc,
  ed.lines.map = (u32*)malloc(sizeof(u32) * lc);
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

// moves the cursor vertically up by `steps` times. The lock_cursx state
// should be set before calling this method inorder to preserve cursor position
// in x direction.
void editor_curs_mov_up(Editor* ed, GapBuffer* gap, u16 steps) {
  if (ed->curs.y == 0) return;
  steps = (steps > ed->curs.y) ? ed->curs.y : steps;
  u16 target_lno = ed->curs.y - steps;
  u16 target_ln_len = editor_lnend(ed,gap, target_lno) - ed->lines.map[target_lno];
  u16 target_pos = ed->lines.map[target_lno] + min(ed->curs.x, target_ln_len);
  while (gap->c > target_pos) gap_left(gap);
}

// moves the cursor vertically down by `steps` times. The lock_cursx state
// should be set before calling this method inorder to preserve cursor position
// in x direction.
void editor_curs_mov_down(Editor* ed, GapBuffer* gap, u16 steps) {
  if (ed->curs.y == ed->lines.count) return;
  steps = (ed->curs.y + steps > ed->lines.count - 1) ? ed->lines.count - ed->curs.y - 1 : steps;
  u16 target_lno = ed->curs.y + steps;
  u16 target_ln_len = editor_lnend(ed, gap, target_lno) - ed->lines.map[target_lno];
  u16 target_pos = ed->lines.map[target_lno] + min(ed->curs.x, target_ln_len);
  while (gap->c < target_pos) gap_right(gap);
}

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

void editor_update_lines(Editor* ed, GapBuffer* gap) {
  if (GAPBUF_LEN(gap) == 0) {
    ed->lines.count = 0;
    return;
  }
  lc_t lc = 0;
  ed->lines.map[lc] = 0;
  u32 len = GAPBUF_LEN(gap);
  for (u32 i = 0; i < len; i++) {
    if (gap_getch(gap, i) == '\n') {
      ed->lines.map[++lc] = i+1;
      lines_increment_count(&ed->lines);
    }
  }
  ed->lines.count = gap_getch(gap, len - 1) == '\n' ? lc : lc + 1;
}

void editor_update_curs(Editor* ed, GapBuffer* gap) {
  lc_t curs_lno = 0;
  for (u32 i = 0; i < gap->c; i++) {
    if (gap->start[i] == '\n') curs_lno++;
  }
  ed->curs.y = curs_lno;
  editor_update_cursx(ed, gap->c - ed->lines.map[curs_lno]);
}

// updates the view component of editor, expects an updated curs component
void editor_update_view(Editor* ed, GapBuffer* gap) {
  while (ed->curs.y - ed->view.y > LINES - 1 - SCROLL_MARGIN) {
    ed->view.offset = ed->lines.map[++ed->view.y];
    editor_update_curs(ed, gap);
  }
  while (ed->view.y > 0 && ed->curs.y < ed->view.y + SCROLL_MARGIN) {
    ed->view.offset = ed->lines.map[--ed->view.y];
    editor_update_curs(ed, gap);
  }
}

// renders the gap buffer as desired, expect an updated view component
void editor_draw(Editor* ed, GapBuffer* gap) {
  u32* ln_map = ed->lines.map;
  erase();
  for (u32 ln = 0; ln < LINES  && ln < ed->lines.count; ln++) {
    u32 lnend = editor_lnend(ed, gap, ln + ed->view.y);
    if (lnend != LN_EMPTY) {
      for (u32 i = ln_map[ln + ed->view.y]; i <= lnend; i++) {
        addch(gap_getch(gap, i));
      }
    }
  }
  move(ed->curs.y - ed->view.y, gap->c - ln_map[ed->curs.y]);
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
        editor_update_lines(&ed, &gap);
        break;
      case KEY_ENTER:
        gap_insertc(&gap, '\n');
        editor_update_lines(&ed, &gap);
        break;
      default:
        gap_insertc(&gap, ch);
        editor_update_lines(&ed, &gap);
        break;
    }
    editor_update_curs(&ed, &gap);
    editor_update_view(&ed, &gap);
    editor_draw(&ed, &gap);
    refresh();
  }
  
  endwin();
  editor_free(&ed);
  gap_free(&gap);
  return 0;
}
