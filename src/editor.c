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
#include "include/gap.h"
#include "include/err.h"
#include "include/vector.h"

VECTOR(u32, u32);

#define SCROLL_MARGIN 3
#define LNO_PADDING 8
#define MAX_SNAPS 1000
#define TAB_WIDTH 2

struct point { 
  u32 x;
  u32 y;
}; 

enum timeline_op { idle, ins, del };

struct snap { enum timeline_op op; u32 start; u32 len; u32 *frame; };

struct timeline_c {
  struct {
    u32Vec content;
    u32 start; // start index
  } trace;
  struct snap undo[MAX_SNAPS];
  struct snap redo[MAX_SNAPS];
  i32 utop;
  i32 rtop;
  bool is_full; // utop and rtop treats undo and redo as circular array if this is set
};

typedef enum ed_states {
  st_lock_cursx = 0x1, // prevent writing to editor.curs.x; method: editor_update_cursx()
  st_buffer_empty = 0x2,
  st_modified = 0x4,
  _mask = 0xffff,
} st_t;

typedef struct {
  st_t states; // stores all states used in the editor.
  GapBuffer gap;
  struct point curs;
  struct point view;
  u32Vec lines;
  err_handler_t error;
  struct timeline_c timeline;
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


/** @states **/
static inline void st_set(Editor* ed, st_t new_states) { ed->states = ed->states | new_states; }
static inline void st_reset(Editor* ed, st_t new_states) { ed->states = ed->states & ( new_states ^ _mask); }
static inline bool st_has(Editor* ed, st_t states) { return (ed->states & states) == states; }


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
  u32 len = GAP_LEN(&ed->gap);
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
static inline void update_cursx(Editor* ed, u16 pos) { if (!st_has(ed, st_lock_cursx)) ed->curs.x = pos; }

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

/** @SNAPS **/
// inorder to create a snap frame, the trace field should be recorded initially
static struct snap snap_init(Editor* ed) {
  u32 trace_start = ed->timeline.trace.start;
  u32 trace_c = ed->gap.c;

  enum timeline_op op = idle;
  u32 start = MIN(trace_start, trace_c);
  u32 len = ABSDIFF(trace_start, trace_c);
  u32* frame = NULL;

  if (trace_start < trace_c) op = ins;
  else if (trace_start > trace_c) op = del;

  if (op != idle) {
    frame = (u32*)malloc(sizeof(u32) * len);
    if (frame == NULL) ed->error(editor_err_malloc_failure, NULL);
    for (u32 i = 0; i < len; i++) {
      frame[i] = u32Vec_get(&ed->timeline.trace.content, i);
    }
  }
  return (struct snap) {op, start, len, frame};
}

static inline void snap_free(struct snap* snap) {
  free(snap->frame);
  *snap = (struct snap){0};
}

/** @TIMELINE **/
static struct timeline_c timeline_init() {
  struct timeline_c tl = {.rtop = -1, .utop = -1};
  tl.trace.content = u32Vec_init(256, NULL);
  return tl;
}

void timeline_free(struct timeline_c* tl) {
  u32Vec_free(&tl->trace.content);
  for (i32 i = 0; i <= tl->utop; i++) { snap_free(&tl->undo[i]); }
  for (i32 i = 0; i <= tl->rtop; i++) { snap_free(&tl->undo[i]); }
}

static inline void timeline_trace_reset(Editor* ed) {
  ed->timeline.trace.start = 0;
  u32Vec_reset(&ed->timeline.trace.content);
}

/** @EDITOR **/
static void editor_handle_implicit_states(Editor* ed, u32 ch) {
  if (st_has(ed, st_modified)) {
    // handle undo redo trace mechanism
  }
  switch (ch) {
    case KEY_UP:
    case KEY_DOWN: st_set(ed, st_lock_cursx); return;
  }
  st_reset(ed, st_lock_cursx | st_modified);
}

static Editor* editor_init() {
  Editor* ed = malloc(sizeof(Editor));
  if (ed == NULL) editor_err_handler(editor_err_malloc_failure, NULL);
  *ed = (Editor){0};
  ed->error = editor_err_handler;
  ed->timeline = timeline_init();

  ed->gap = gap_init(GAP_RESIZE_STEP);

  ed->lines = u32Vec_init(1024, NULL);
  u32Vec_insert(&ed->lines, 0, 0);

  st_set(ed, st_buffer_empty);
  return ed;
}

static void editor_free(Editor** ed) {
  u32Vec_free(&(*ed)->lines);
  gap_free(&(*ed)->gap);
  timeline_free(&(*ed)->timeline);

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
  st_set(ed, st_modified);
  if (st_has(ed, st_buffer_empty)) { st_reset(ed, st_buffer_empty); }
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
  st_set(ed, st_modified);
  if (GAP_LEN(&ed->gap) == 0) { st_set(ed, st_buffer_empty); }
}

static void editor_draw(WINDOW* edwin, Editor* ed) {
  u16 win_h, win_w;
  getmaxyx(edwin, win_h, win_w);
  update_view(ed, win_h, win_w);

  werase(edwin);
  mvwvline(edwin, 0, 0, ACS_VLINE, win_h);
  mvwvline(edwin, 0, win_w - 1, ACS_VLINE, win_h);
  mvwprintw(edwin, 0, 1, "%5d  ", ed->view.y + 1);

  if (st_has(ed, st_buffer_empty)) {
    char* msg = "ctrl + q to quit";
    mvwprintw(edwin, CENTER(win_h, 3), CENTER(win_w, strlen(msg)), "%s", msg);
    wmove(edwin, 0, LNO_PADDING);
    return;
  }

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
      editor_handle_implicit_states(ed, ch);
      if (ch >= 32 && ch < 127) { // ascii printable character range
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
          case '\t' : editor_insertch(ed, '\t'); break; // todo
          case CTRL('u'): editor_insertch(ed, 'U');break;
          case CTRL('r'): editor_insertch(ed, 'R');break;
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
#undef TAB_WIDTH
