/*
TODO
 - up and down movements [v]
 - snap scrolling [v]
 - split editor operations into component level [v]
 - make editior updation incremental [v] NOTE I can do this using gap buffer
 - make Editor window attachable [v]
 - handle horizontal text overflows [v]
 - make a header file and compile this separately once i am done with this[_]
 - undo-redo [_]
*/

#include <time.h>
#include <ncurses.h>
#include <stdbool.h>
#include "include/ated.h"
#include "include/itypes.h"

#define GAP_RESIZE_STEP KB(4)
#include "gap_buffer.c"

// lc_t will specify the maximum number of lines possible.
#define lc_t u16

#define SCROLL_MARGIN 3
#define LNO_PADDING 8
#define MAX_SNAPS 1000

// An "_c" in the following struct names can be abbreviated as _component.
 
// A line is an abstraction over the stream of text. Enables performing
// operations on editor at line level. line map is like an associative array.
// array index is line number, and its content is start position of line
// inside the buffer.
struct lines_c { 
  u32 *map; // lines lookup table; need mem reallocation
  lc_t count;
  lc_t capacity;
};

struct point { 
  u32 x;
  u32 y;
}; 

struct snap { i8 op; u32 curs; u32 len; u8 *frame; };
struct snapshots_c {
  struct timespec last_written;
  struct snap undo[MAX_SNAPS];
  struct snap redo[MAX_SNAPS];
  i16 utop; // empty => -1
  i16 rtop;
  bool is_full; // utop and rtop treats undo and redo as circular array if this is set
};

// NOTE: undo->undo->redo->modify => erase redo

enum editor_modifiers {
  lock_cursx = 0x1, // prevent writing to editor.curs.x; method: editor_update_cursx()
  _mask = 0xffff,
};

enum undo_redo_operations {
  insertch = 1,
  lremovech = -1,
};

typedef struct {
  u16 mods; // stores all modifiers used in the editor.
  // states should only be modified through the designated methods.
  GapBuffer gap;
  struct point curs;
  struct point view;
  struct lines_c lines;
  struct snapshots_c snaps; // for undo-redo
} Editor;


/** @MODIFIER **/
static inline void set_mods(Editor* ed, u16 new_mods) { ed->mods = ed->mods | new_mods; }
static inline void reset_mods(Editor* ed, u16 new_mods) { ed->mods = ed->mods & ( new_mods ^ _mask); }
static inline bool has_mods(Editor* ed, u16 mods) { return (ed->mods & mods) == mods; }

static void handle_implicit_mods(Editor* ed, u32 ch) {
  switch (ch) {
    case KEY_UP:
    case KEY_DOWN: set_mods(ed, lock_cursx); return;
  }
  reset_mods(ed, lock_cursx);
}


/** @LINES **/
// the following functions return logical indices from lines.map.
u32 static inline lnstart(Editor* ed, lc_t lno) { return ed->lines.map[lno]; }
u32 static inline viewln(Editor* ed, lc_t ln_offset) { return ed->lines.map[ln_offset + ed->view.y]; }
u32 static inline cursln(Editor* ed, lc_t ln_offset) { return ed->lines.map[ln_offset + ed->curs.y]; }

#define LN_EMPTY U32_MAX
// returns logical end position of given lno from line component of editor.
// returns LN_EMPTY for empty lines
static u32 lnend(Editor* ed, lc_t lno) {
  u32 i = lnstart(ed, lno);
  u32 len = GAPBUF_LEN(&ed->gap);
  if (i >= len) return LN_EMPTY;
  while (i < len - 1 && gap_getch(&ed->gap, i) != '\n') i++;
  return i;
}

// the following 4 operations expects curs_c before updation.

static inline void increment_next_lines(Editor* ed) { for (lc_t i = ed->curs.y + 1; i < ed->lines.count; i++) ed->lines.map[i]++; }
static inline void decrement_next_lines(Editor* ed) { for (lc_t i = ed->curs.y + 1; i < ed->lines.count; i++) ed->lines.map[i]--; }

static void increment_lc(Editor* ed) {
  struct lines_c* lines = &ed->lines;
  if (lines->count >= lines->capacity) {
    lines->capacity += KB(1);
    lines->map = realloc(lines->map, sizeof(u32) * lines->capacity);
    if (!lines->map) {
      perror("failed to do realloc for lines.map");
      exit(-1);
    }
  }
  lines->count++;
}

static void add_newline(Editor* ed, u32 logical_pos) {
  increment_lc(ed);
  for (lc_t i = ed->lines.count - 1; i > ed->curs.y + 1; i--) {
    ed->lines.map[i] = ed->lines.map[i - 1]; // shifting right from curs_lno + 1 pos
  }
  // curs_lno + 1 position should be unoccupied now
  ed->lines.map[ed->curs.y + 1] = logical_pos; // new position is inserted there
}

static void remove_newline(Editor* ed) {
  for (lc_t i = ed->curs.y + 1; i < ed->lines.count; i++) {
    ed->lines.map[i - 1] = ed->lines.map[i];
  }
  ed->lines.count--;
}


/** @VIEW **/
// curs_c must be updated before calling this method
static void update_view(Editor* ed, u16 win_h, u16 win_w) {
  while (ed->curs.y - ed->view.y > win_h - SCROLL_MARGIN - 1) ed->view.y++; // downwards
  while (ed->view.y > 0 && ed->curs.y < ed->view.y + SCROLL_MARGIN) ed->view.y--; // upwards
  u16 curs_len = lnend(ed, ed->curs.y) - cursln(ed, 0);
  u16 curs_pos = ed->gap.c - cursln(ed, 0);
  while (ed->view.x < curs_len && curs_pos - ed->view.x >= win_w - LNO_PADDING - SCROLL_MARGIN - 1) ed->view.x++; // rightwards
  while (ed->view.x > 0 && curs_pos <= ed->view.x + SCROLL_MARGIN) ed->view.x--; // leftwards
}

/** @CURS **/
static inline void update_cursx(Editor* ed, u16 pos) { if (!has_mods(ed, lock_cursx)) ed->curs.x = pos; }

static void curs_mov_left(Editor* ed) {
  GapBuffer* gap = &ed->gap;
  if (gap->c > 0) {
    u8 prev_ch = gap->start[gap->c - 1];
    if (prev_ch == '\n') {
      ed->curs.y--;
    }
    gap_left(gap, 1);
    update_cursx(ed, gap->c - cursln(ed, 0));
  }
}

static void curs_mov_right(Editor* ed) {
  u8 next_ch = gap_getch(&ed->gap, ed->gap.c);
  if (next_ch != 0) {
    if (next_ch == '\n') ed->curs.y++;
    gap_right(&ed->gap, 1);
    update_cursx(ed, ed->gap.c - cursln(ed, 0));
  }
}

static void curs_mov_up(Editor* ed, u16 times) {
  if (ed->curs.y == 0) return;
  times = (times > ed->curs.y) ? ed->curs.y : times;
  u16 target_lno = ed->curs.y - times;
  u32 target_end = lnend(ed, target_lno);
  if (target_end == LN_EMPTY) return;
  u16 target_len = target_end - lnstart(ed, target_lno);
  u32 target_pos = lnstart(ed, target_lno) + MIN(ed->curs.x, target_len);
  gap_left(&ed->gap, ed->gap.c - target_pos);
  ed->curs.y -= times;
}

static void curs_mov_down(Editor* ed, u16 times) {
  if (ed->curs.y >= ed->lines.count - 1) return;
  times = (ed->curs.y + times > ed->lines.count - 1) ? ed->lines.count - ed->curs.y - 1 : times;
  u16 target_lno = ed->curs.y + times;
  u32 target_end = lnend(ed, target_lno);
  if (target_end == LN_EMPTY) return;
  u16 target_len = target_end - lnstart(ed, target_lno);
  u32 target_pos = lnstart(ed, target_lno) + MIN(ed->curs.x, target_len);
  gap_right(&ed->gap, target_pos - ed->gap.c);
  ed->curs.y += times;
}

/** @SNAPSHOTS **/



/** @EDITOR **/
static Editor* editor_init() {
  Editor* ed = malloc(sizeof(Editor));
  memset(ed, 0, sizeof(Editor));
  ed->gap = gap_init(GAP_RESIZE_STEP);
  ed->lines.capacity = KB(1);
  ed->lines.count = 1;
  ed->lines.map = (u32*)malloc(sizeof(u32) * ed->lines.capacity);
  if (!ed->lines.map) {
    perror("failed to do malloc for lines.map");
    exit(-1);
  }
  memset(ed->lines.map, 0, sizeof(u32) * ed->lines.capacity);
  ed->snaps.utop = ed->snaps.rtop = -1;
  ed->snaps.is_full = false;
  clock_gettime(CLOCK_MONOTONIC, &ed->snaps.last_written);
  return ed;
}

static void editor_free(Editor** ed) {
  free((*ed)->lines.map);
  gap_free(&(*ed)->gap);
  free(*ed);
  *ed = NULL;
}

static void editor_insertch(Editor* ed, u32 ch) {
  gap_insertch(&ed->gap, ch);
  increment_next_lines(ed);
  if (ch == '\n') {
    add_newline(ed, ed->gap.c);
    ed->curs.y++;
  }
  update_cursx(ed, ed->gap.c - cursln(ed, 0));
}

static void editor_removech(Editor* ed) {
  u8 removing_ch = gap_getch(&ed->gap, ed->gap.c - 1);
  if (!removing_ch) return;
  decrement_next_lines(ed);
  gap_removech(&ed->gap);
  if (removing_ch == '\n') {
    remove_newline(ed);
    ed->curs.y--;
  }
  update_cursx(ed, ed->gap.c - cursln(ed, 0));
}

static void editor_draw(WINDOW* edwin, Editor* ed) {
  u16 win_h, win_w;
  getmaxyx(edwin, win_h, win_w);
  update_view(ed, win_h, win_w);

  werase(edwin);
  mvwvline(edwin, 0, 0, ACS_VLINE, win_h);
  mvwvline(edwin, 0, win_w - 1, ACS_VLINE, win_h);
  mvwprintw(edwin, 0, 1, "%5d ", ed->view.y + 1);

  for (u32 ln = 0; ln < win_h  && ln + ed->view.y < ed->lines.count; ln++) {
    u32 end = lnend(ed, ln + ed->view.y);
    if (end == LN_EMPTY) continue;
    u32 len = end - lnstart(ed, ln + ed->view.y) + 1;
    mvwprintw(edwin, ln, 1, "%5d ", ln + ed->view.y + 1);

    for (u16 x = 0; x + ed->view.x < len && x + LNO_PADDING < win_w - 1; x++) {
      u8 ch = gap_getch(&ed->gap, x + viewln(ed, ln) + ed->view.x);
      if (ch == '\n') break;
      if (ch) mvwaddch(edwin, ln, x + LNO_PADDING, ch);
    }

    mvwprintw(edwin, ln + 1, 1, "%5d ", ln + ed->view.y + 2);
  }
  wmove(edwin, ed->curs.y - ed->view.y, MIN(win_w - 1 ,ed->gap.c - cursln(ed, 0) + LNO_PADDING - ed->view.x));
}


/** @ENTRY_POINT **/
void editor_process(WINDOW* edwin) {
  Editor *ed = editor_init();
  keypad(edwin, TRUE);

  u32 ch;
  editor_draw(edwin, ed);
  wrefresh(edwin);
  while ((ch = wgetch(edwin)) != CTRL('q')) {
    if (ch != ERR) {
      handle_implicit_mods(ed, ch);
      if (ch >= 32 && ch < 127) { // printable characters
        editor_insertch(ed, ch);
      } else {
        switch (ch) {
          case KEY_LEFT: curs_mov_left(ed); break;
          case KEY_RIGHT: curs_mov_right(ed); break;
          case KEY_UP: curs_mov_up(ed, 1); break;
          case KEY_DOWN: curs_mov_down(ed, 1); break;
          case KEY_BACKSPACE: editor_removech(ed);
            break;
          case KEY_ENTER: case '\n': editor_insertch(ed, '\n'); break;
        }
      }
      editor_draw(edwin, ed);
    }
    wrefresh(edwin);
  }

  editor_free(&ed);
}

#undef lc_t
#undef SCROLL_MARGIN
#undef LN_EMPTY

