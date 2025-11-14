#include <time.h>
#include <ncurses.h>
#include "include/utils.h"
#include "include/itypes.h"
#include "include/gap.h"
#include "include/err.h"
#include "include/vector.h"

VECTOR(u32, u32);

#define SCROLL_MARGIN 3
#define LNO_PADDING 8

#define TAB_WIDTH 2

#define UNDO_LIMIT 1024
#define UNDO_EXPIRY MSEC(650)
#define STK_EMTY -1

#define LN_EMPTY U32_MAX

struct point { 
  u32 x;
  u32 y;
}; 

enum timeline_op {
  op_idle = 0,
  op_ins = 1,
  op_del = -1
};

struct action {
  enum timeline_op op;
  u32 start;
  u32Vec frame;
};

struct timeline {
  struct timespec time;
  struct action undo[UNDO_LIMIT];
  struct action redo[UNDO_LIMIT];
  isize utop;
  isize rtop;
};

enum editor_ctrl {
  st_lock_cursx = 0x1, // prevent writing to editor.curs.x for sticky cursor behaviour
  blank = 0x2, // indicates the editor text buffer is empty
  undoing = 0x4, // editor is performing an undo or redo operation
  commit_action = 0x8, // to force commit current timeline to undo
  _mask = 0xffffffff,
};

typedef struct {
  enum editor_ctrl ctrl;
  GapBuffer gap;
  struct point curs;
  struct point view;
  u32Vec lines;
  err_handler_t error;
  struct timeline timeline;
} Editor;

// reverses u32Vec
static void u32Vec_rev(u32Vec* vec) {
  for (isize i = 0; i < vec->len / 2; i++) {
    u32 beg_val = u32Vec_get(vec, i);
    u32 end_val = u32Vec_get(vec, _END(i));
    u32Vec_set(vec, end_val, i);
    u32Vec_set(vec, beg_val, _END(i));
  }
}

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
static inline void _set(enum editor_ctrl* st, enum editor_ctrl set) { *st = *st | set; }
static inline void _reset(enum editor_ctrl* st, enum editor_ctrl reset) { *st = *st & ( reset ^ _mask); }
static inline bool _has(enum editor_ctrl st, enum editor_ctrl has) { return (st & has) == has; }


/** @LINES **/
// the following functions return logical indices from lines.map.
static inline u32 lnstart(Editor* ed, u32 lno) { return u32Vec_get(&ed->lines, lno); }
static inline u32 viewln(Editor* ed, u32 ln_offset) { return u32Vec_get(&ed->lines, ln_offset + ed->view.y); }
static inline u32 cursln(Editor* ed, u32 ln_offset) { return u32Vec_get(&ed->lines, ln_offset + ed->curs.y); }

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
  while (DELTA(ed->view.y, ed->curs.y) > win_h - SCROLL_MARGIN - 1) ed->view.y++; // downwards
  while (ed->view.y > 0 && ed->curs.y < ed->view.y + SCROLL_MARGIN) ed->view.y--; // upwards
  u32 end = lnend(ed, ed->curs.y);
  if (end == LN_EMPTY) return;
  u32 curs_len = end - cursln(ed, 0);
  u32 curs_pos = ed->gap.c - cursln(ed, 0);
  while (ed->view.x < curs_len && curs_pos - ed->view.x >= win_w - LNO_PADDING - SCROLL_MARGIN - 1) ed->view.x++; // rightwards
  while (ed->view.x > 0 && curs_pos <= ed->view.x + SCROLL_MARGIN) ed->view.x--; // leftwards
}

/** @CURS **/
static inline void update_cursx(Editor* ed, u16 pos) { if (!_has(ed->ctrl, st_lock_cursx)) ed->curs.x = pos; }

static void _curs_mov_vertical(Editor* ed, i32 times) {
  if (times == 0) return;
  if (times > 0) {
    if (ed->curs.y == 0) return;
    times = MIN(times, ed->curs.y);
  } else if (times < 0) {
    if (ed->curs.y >= ed->lines.len - 1) return;
    times = (ed->curs.y + -times > ed->lines.len - 1) ? -(ed->lines.len - ed->curs.y - 1) : times;
  }
  u32 target_lno = ed->curs.y - times;
  u32 target_end = lnend(ed, target_lno);
  if (target_end == LN_EMPTY) return;
  u32 target_len = target_end - lnstart(ed, target_lno);
  u32 target_pos = lnstart(ed, target_lno) + MIN(ed->curs.x, target_len);
  gap_move(&ed->gap, target_pos);
  ed->curs.y = target_lno;
}

static inline void curs_mov_up(Editor* ed, u16 times) { _curs_mov_vertical(ed, times); }
static inline void curs_mov_down(Editor* ed, u16 times) { _curs_mov_vertical(ed, -times); }

static void curs_mov_left(Editor* ed, u32 times) {
  while (ed->gap.c > 0 && times > 0) {
    if (gap_getch(&ed->gap, ed->gap.c - 1) == '\n') ed->curs.y--;
    gap_left(&ed->gap, 1);
    times--;
  }
  update_cursx(ed, ed->gap.c - cursln(ed, 0));
}

static void curs_mov_right(Editor* ed, u32 times) {
  while (ed->gap.c < GAP_LEN(&ed->gap) && times > 0) {
    if (gap_getch(&ed->gap, ed->gap.c) == '\n') ed->curs.y++;
    gap_right(&ed->gap, 1);
    times--;
  }
  update_cursx(ed, ed->gap.c - cursln(ed, 0));
}

static void curs_mov(Editor* ed, u32 pos) {
  if (ed->gap.c < pos) {
    curs_mov_right(ed, pos - ed->gap.c);
  } else if (ed->gap.c > pos) {
    curs_mov_left(ed, ed->gap.c - pos);
  }
}

/** @ACTION **/
// inorder to create a action frame, the trace field should be recorded initially
static inline struct action action_init(u32 start, enum timeline_op op) {
  return (struct action) {
    .frame = u32Vec_init(32, NULL),
    .op = op,
    .start = start,
  };
}

static inline void action_free(struct action* action) {
  u32Vec_free(&action->frame);
  *action = (struct action){0};
}

static void inline action_push(struct action* stack, isize* top, struct action new) {
  isize i = (++(*top)) % UNDO_LIMIT;
  if (*top >= UNDO_LIMIT && stack[i].op != op_idle) { // need to avoid dangling ptrs when going in circle.
    action_free(&stack[i]);
  }
  stack[i] = new;
}

// will return source reference instead of a copy
static inline struct action* action_pop(struct action* stack, isize* top) {
  if (*top == STK_EMTY) return NULL;
  isize i = *top % UNDO_LIMIT;
  *top = i == 0 ? STK_EMTY : *top - 1;
  return &stack[i];
}

/** @TIMELINE **/
static inline void timeline_fetch_time(Editor* ed) { clock_gettime(CLOCK_MONOTONIC, &ed->timeline.time); }

static struct timeline timeline_init() {
  struct timeline tl = {0};
  tl.utop = STK_EMTY;
  tl.rtop = STK_EMTY;
  clock_gettime(CLOCK_MONOTONIC, &tl.time);
  return tl;
}

void timeline_clear_redo(struct timeline* tl) {
  if (tl->rtop == STK_EMTY) return;
  for (u32 i = 0; i < UNDO_LIMIT; i++) {
    if (tl->redo[i].op != 0) {
      action_free(&tl->redo[i]);
    }
  }
  tl->rtop = STK_EMTY;
}

static void editor_update_timeline(Editor* ed, u32 ch, enum timeline_op op) {
  if (_has(ed->ctrl, undoing)) return;
  if (ed->timeline.rtop != STK_EMTY) timeline_clear_redo(&ed->timeline);
  struct action* undo = ed->timeline.undo;
  isize* top = &ed->timeline.utop;
  if (*top == -1 || _has(ed->ctrl, commit_action) || elapsed_seconds(&ed->timeline.time) > UNDO_EXPIRY || op != undo[*top % UNDO_LIMIT].op) {
    struct action new = action_init(ed->gap.c, op);
    action_push(undo, top, new);
    _reset(&ed->ctrl, commit_action);
  }
  u32Vec_insert(&undo[*top % UNDO_LIMIT].frame, ch, _END(0));
  timeline_fetch_time(ed);
}

void timeline_free(struct timeline* tl) {
  timeline_clear_redo(tl);
  for (u32 i = 0; i < UNDO_LIMIT; i++) {
    if (tl->undo[i].op != 0) {
      action_free(&tl->undo[i]);
    }
  }
}

/** @EDITOR **/
static void editor_handle_implicit_states(Editor* ed, u32 ch) {
  _reset(&ed->ctrl, undoing);
  switch (ch) {
    case KEY_LEFT:
    case KEY_RIGHT: _set(&ed->ctrl, commit_action); break;
    case KEY_UP:
    case KEY_DOWN: _set(&ed->ctrl, st_lock_cursx | commit_action); return;
  }
  _reset(&ed->ctrl, st_lock_cursx);
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

  _set(&ed->ctrl, blank);
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
  if (!_has(ed->ctrl, undoing)) editor_update_timeline(ed, ch, op_ins);
  gap_insertch(&ed->gap, ch);
  adjust_lines_after_insert(ed);
  if (ch == '\n') {
    ed->curs.y++;
    update_lines_after_newline(ed);
  }
  update_cursx(ed, ed->gap.c - cursln(ed, 0));
  if (_has(ed->ctrl, blank)) {
    _reset(&ed->ctrl, blank);
  }
}

static void editor_removech(Editor* ed) {
  u32 removing_ch = gap_getch(&ed->gap, ed->gap.c - 1);
  if ((u8)removing_ch == 0) return;
  if (!_has(ed->ctrl, undoing)) {
    editor_update_timeline(ed, removing_ch, op_del);
  }
  adjust_lines_after_remove(ed);
  gap_removech(&ed->gap);
  if (removing_ch == '\n') {
    update_after_line_removal(ed);
    ed->curs.y--;
  }
  update_cursx(ed, ed->gap.c - cursln(ed, 0));
  if (GAP_LEN(&ed->gap) == 0) { _set(&ed->ctrl, blank); }
}

static void timeline_invert_action(Editor* ed, struct action* action) {
  if (action->op == op_idle) return;

  action->start += action->op * action->frame.len;
  curs_mov(ed, action->start);
  if (action->op == op_ins) {
    for (isize i = 0; i < action->frame.len; i++) editor_removech(ed);
  } else if (action->op == op_del) {
    for (isize i = 0; i < action->frame.len; i++) {
      editor_insertch(ed, u32Vec_get(&action->frame, _END(i)));
    }
  }
  action->op *= -1;
  u32Vec_rev(&action->frame);
}

void editor_undo(Editor* ed) {
  _set(&ed->ctrl, undoing);
  struct action* action = action_pop(ed->timeline.undo, &ed->timeline.utop);
  if (action == NULL) return;
  timeline_invert_action(ed, action);
  action_push(ed->timeline.redo, &ed->timeline.rtop, *action);
  action->op = op_idle;
}

void editor_redo(Editor* ed) {
  _set(&ed->ctrl, undoing);
  struct action* action = action_pop(ed->timeline.redo, &ed->timeline.rtop);
  if (action == NULL) return;
  timeline_invert_action(ed, action);
  action_push(ed->timeline.undo, &ed->timeline.utop, *action);
  action->op = op_idle;
}

static inline void editor_help(WINDOW* win, u16 w, u16 h) {
  char* msg = "ctrl + q to quit.";
  mvwprintw(win, CENTER(h, 0), CENTER(w, strlen(msg)), "%s", msg);
  wmove(win, 0, LNO_PADDING);
}

static void editor_draw(WINDOW* edwin, Editor* ed) {
  u16 win_h, win_w;
  getmaxyx(edwin, win_h, win_w);
  update_view(ed, win_h, win_w);

  werase(edwin);
  mvwvline(edwin, 0, 0, ACS_VLINE, win_h);
  mvwprintw(edwin, 0, 1, "%5d  ", ed->view.y + 1);

  if (_has(ed->ctrl, blank)) {
    editor_help(edwin, win_w, win_h);
    return;
  }

  for (u32 ln = 0; ln < win_h  && ln + ed->view.y < ed->lines.len; ln++) {
    u32 end = lnend(ed, ln + ed->view.y);
    if (end == LN_EMPTY) continue;
    u32 len = end - lnstart(ed, ln + ed->view.y) + 1;
    mvwprintw(edwin, ln, 1, "%5d ", ln + ed->view.y + 1);

    for (u16 x = 0; x + ed->view.x < len && x + LNO_PADDING < win_w; x++) {
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
          case KEY_LEFT: curs_mov_left(ed, 1); break;
          case KEY_RIGHT: curs_mov_right(ed, 1); break;
          case KEY_UP: curs_mov_up(ed, 1); break;
          case KEY_DOWN: curs_mov_down(ed, 1); break;
          case KEY_BACKSPACE: editor_removech(ed); break;
          case KEY_ENTER: case '\n': editor_insertch(ed, '\n'); break;
          case '\t' : editor_insertch(ed, '\t'); break;
          case CTRL('u'): editor_undo(ed); break;
          case CTRL('r'): editor_redo(ed) ;break;
        }
      }
      editor_draw(edwin, ed);
    }
    wrefresh(edwin);
  }

  editor_free(&ed);
}
