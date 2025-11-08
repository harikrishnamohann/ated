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

// #include <time.h>
#include <ncurses.h>
#include <stdbool.h>
#include "include/utils.h"
#include "include/itypes.h"
#include "gap_buffer.c"
#include "include/err.h"
#include "include/vector.h"

VECTOR(u32, u32);

#define SCROLL_MARGIN 3
#define LNO_PADDING 8
#define MAX_SNAPS 1000

struct point { 
  u32 x;
  u32 y;
}; 

// struct snap { i8 op; u32 start; u32 len; u32 *frame; };

// enum undo_redo_operations {
//   insertch = 1,
//   removech = -1,
// };

// struct snapshots_c {
//   struct {
//     u32Vec content;
//     u32 start; // start index
//   } trace;
//   struct snap undo[MAX_SNAPS];
//   struct snap redo[MAX_SNAPS];
//   u16 utop;
//   u16 rtop;
//   bool is_full; // utop and rtop treats undo and redo as circular array if this is set
// };

// NOTE: undo->undo->redo->modify => erase redo
typedef u32 mods_t;

enum editor_mods {
  lock_cursx = 0x1, // prevent writing to editor.curs.x; method: editor_update_cursx()
  _mask = 0xffff,
};

typedef struct {
  mods_t mods; // stores all modifiers used in the editor.
  GapBuffer gap;
  struct point curs;
  struct point view;
  u32Vec lines;
  err_handler_t error;
} Editor;

// @ERRORS
static enum {
  editor_err_ok,
  editor_err_malloc_failure,
} ederr;

void editor_err_handler(i32 errno, void* args) {
  switch(errno) {
    case editor_err_malloc_failure:
      perror("A memory allocation in editor failed.");
      endwin();
      exit(-1);
    case editor_err_ok:
      return;
  }
}


/** @MODS **/
static inline void mods_set(Editor* ed, mods_t new_mods) { ed->mods = ed->mods | new_mods; }
static inline void mods_reset(Editor* ed, mods_t new_mods) { ed->mods = ed->mods & ( new_mods ^ _mask); }
static inline bool mods_has(Editor* ed, mods_t mods) { return (ed->mods & mods) == mods; }

static void handle_implicit_mods(Editor* ed, u32 ch) {
  switch (ch) {
    case KEY_UP:
    case KEY_DOWN: mods_set(ed, lock_cursx); return;
  }
  mods_reset(ed, lock_cursx);
}


/** @LINES **/
// the following functions return logical indices from lines.map.
static inline u32 lnstart(Editor* ed, u32 lno) { return u32Vec_get(&ed->lines, lno); }
static inline u32 viewln(Editor* ed, u32 ln_offset) { return u32Vec_get(&ed->lines, ln_offset + ed->view.y); }
static inline u32 cursln(Editor* ed, u32 ln_offset) { return u32Vec_get(&ed->lines, ln_offset + ed->curs.y); }

#define LN_EMPTY U32_MAX
// returns logical end position of given lno from line component of editor.
// returns LN_EMPTY for empty lines
static u32 lnend(Editor* ed, u32 lno) {
  u32 i = lnstart(ed, lno);
  u32 len = GAPBUF_LEN(&ed->gap);
  if (i > len) return LN_EMPTY;
  while (i < len && gap_getch(&ed->gap, i) != '\n') i++;
  return i;
}

// the following 4 operations expects curs before updation.
static inline void adjust_lines_after_insert(Editor* ed) { for (u32 i = ed->curs.y + 1; i < ed->lines.len; i++) ed->lines._elements[i]++; }
static inline void adjust_lines_after_remove(Editor* ed) { for (u32 i = ed->curs.y + 1; i < ed->lines.len; i++) ed->lines._elements[i]--; }
static inline void update_lines_after_newline(Editor* ed) { u32Vec_insert(&ed->lines, ed->gap.c, ed->curs.y); }
static inline void update_after_line_removal(Editor* ed) { u32Vec_remove(&ed->lines, ed->curs.y); }


/** @VIEW **/
// curs_c must be updated before calling this method
static void update_view(Editor* ed, u16 win_h, u16 win_w) {
  while (ed->curs.y - ed->view.y > win_h - SCROLL_MARGIN - 1) ed->view.y++; // downwards
  while (ed->view.y > 0 && ed->curs.y < ed->view.y + SCROLL_MARGIN) ed->view.y--; // upwards
  u32 end = lnend(ed, ed->curs.y);
  if (end == LN_EMPTY) return;
  u32 curs_len = end - cursln(ed, 0);
  u32 curs_pos = ed->gap.c - cursln(ed, 0);
  while (ed->view.x < curs_len && curs_pos - ed->view.x >= win_w - LNO_PADDING - SCROLL_MARGIN - 1) ed->view.x++; // rightwards
  while (ed->view.x > 0 && curs_pos <= ed->view.x + SCROLL_MARGIN) ed->view.x--; // leftwards
}

/** @CURS **/
static inline void update_cursx(Editor* ed, u16 pos) { if (!mods_has(ed, lock_cursx)) ed->curs.x = pos; }

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
  u32 target_lno = ed->curs.y - times;
  u32 target_end = lnend(ed, target_lno);
  if (target_end == LN_EMPTY) return;
  u32 target_len = target_end - lnstart(ed, target_lno);
  u32 target_pos = lnstart(ed, target_lno) + MIN(ed->curs.x, target_len);
  gap_left(&ed->gap, ed->gap.c - target_pos);
  ed->curs.y -= times;
}

static void curs_mov_down(Editor* ed, u16 times) {
  if (ed->curs.y >= ed->lines.len - 1) return;
  times = (ed->curs.y + times > ed->lines.len - 1) ? ed->lines.len - ed->curs.y - 1 : times;
  u32 target_lno = ed->curs.y + times;
  u32 target_end = lnend(ed, target_lno);
  if (target_end == LN_EMPTY) return;
  u32 target_len = target_end - lnstart(ed, target_lno);
  u32 target_pos = lnstart(ed, target_lno) + MIN(ed->curs.x, target_len);
  gap_right(&ed->gap, target_pos - ed->gap.c);
  ed->curs.y += times;
}

/** @SNAPSHOTS **/
// static struct snap snap_init(i8 op, u32 start, u32 len, GapBuffer* gap);
// static inline void snap_free(struct snap* snap);
// static inline void snapshot_capture(Editor* ed);
// static void snaps_insert(Editor* ed);


/** @EDITOR **/
static Editor* editor_init() {
  Editor* ed = malloc(sizeof(Editor));
  if (ed == NULL) editor_err_handler(editor_err_malloc_failure, NULL);
  *ed = (Editor){0};
  ed->error = editor_err_handler;

  ed->gap = gap_init(GAP_RESIZE_STEP);

  ed->lines = u32Vec_init(1024, NULL);
  u32Vec_insert(&ed->lines, 0, 0);

  return ed;
}

static void editor_free(Editor** ed) {
  u32Vec_free(&(*ed)->lines);
  gap_free(&(*ed)->gap);
  **ed = (Editor){0};
  free(*ed);
  *ed = NULL;
}

static void editor_insertch(Editor* ed, u32 ch) {
  gap_insertch(&ed->gap, ch);
  adjust_lines_after_insert(ed);
  if (ch == '\n') {
    ed->curs.y++;
    update_lines_after_newline(ed);
  }
  update_cursx(ed, ed->gap.c - cursln(ed, 0));
}

static void editor_removech(Editor* ed) {
  u8 removing_ch = gap_getch(&ed->gap, ed->gap.c - 1);
  if (removing_ch == 0) return;
  adjust_lines_after_remove(ed);
  gap_removech(&ed->gap);
  if (removing_ch == '\n') {
    update_after_line_removal(ed);
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

  for (u32 ln = 0; ln < win_h  && ln + ed->view.y < ed->lines.len; ln++) {
    u32 end = lnend(ed, ln + ed->view.y);
    if (end == LN_EMPTY) continue;
    u32 len = end - lnstart(ed, ln + ed->view.y) + 1;
    mvwprintw(edwin, ln, 1, "%5d ", ln + ed->view.y + 1);

    for (u16 x = 0; x + ed->view.x < len && x + LNO_PADDING < win_w - 1; x++) {
      u8 ch = gap_getch(&ed->gap, x + viewln(ed, ln) + ed->view.x);
      if (ch == '\n') break;
      if (ch) mvwaddch(edwin, ln, x + LNO_PADDING, ch);
    }

    mvwprintw(edwin, ln + 1, 1, "    ~");
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

#undef SCROLL_MARGIN
#undef LN_EMPTY
