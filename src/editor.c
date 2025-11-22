#include <time.h>
#include <ncurses.h>
#include "include/utils.h"
#include "include/itypes.h"
#include "include/gap.h"
#include "include/err.h"
#include "include/vector.h"

VECTOR(u32, u32);

#define SCROLL_BOUNDRY 4
#define SCROLL_BOUNDRY_PADDED (SCROLL_BOUNDRY + 2)
#define LNO_PADDING 8

#define TAB_STOPS 8

#define UNDO_LIMIT 1024
#define UNDO_EXPIRY MSEC(650)
#define STK_EMTY -1

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
  sticky_scroll = 0x1, // prevent writing to editor.curs.x for sticky cursor behaviour
  blank = 0x2, // indicates the editor text buffer is empty
  undoing = 0x4, // editor is performing an undo or redo operation
  commit_action = 0x8, // to force commit current timeline to undo
};

typedef struct {
  enum editor_ctrl cw;
  GapBuffer gap;
  struct { u32 x; u32 y; } view;
  GapBuffer lines;
  isize lineDelta;
  err_handler_t error;
  struct timeline tl;
  u32 sticky_curs;
} Editor;


// returns the distance to next tab after pos
static inline u8 next_tab(u32 pos) { return TAB_STOPS - (pos % TAB_STOPS); }

static u32 get_visual_width(Editor* ed, u32 start, u32 end) {
  u32 width = 0;
  for (u32 i = start; i < end; i++) {
    if (gap_getch(&ed->gap, i) == '\t') {
      width += next_tab(width);
    } else {
      width++;
    }
  }
  return width;
}


/** @states **/
static inline void _set(enum editor_ctrl* st, enum editor_ctrl set) { *st = *st | set; }
static inline void _reset(enum editor_ctrl* st, enum editor_ctrl reset) { *st = *st & ~reset; }
static inline bool _has(enum editor_ctrl st, enum editor_ctrl has) { return (st & has) == has; }


/** @LINES **/
// line number of cursor
static inline u32 cursy(Editor* ed) { return ed->lines.c - ((ed->lines.c) ? 1 : 0); }

// total number of lines
static inline u32 lncount(Editor* ed) { return GAP_LEN(&ed->lines); }

// logical index of gap->c
static inline u32 gapc(Editor* ed) { return ed->gap.c; }

// start logical index of line lno
static inline u32 lnbeg(Editor* ed, u32 lno) { return gap_getch(&ed->lines, lno) + (lno > cursy(ed) ? ed->lineDelta : 0); }

// logical index of line end lno
static inline u32 lnend(Editor* ed, u32 lno) { return (lno >= lncount(ed) - 1) ? GAP_LEN(&ed->gap) : lnbeg(ed, lno + 1) - 1; }

// length of a line
static inline u32 lnlen(Editor* ed, u32 lno) { return lnend(ed, lno) - lnbeg(ed, lno); }

// cursor offset relative to line start
static inline u32 cursx(Editor* ed) { return gapc(ed) - lnbeg(ed, cursy(ed)); }

// to insert a line in line component
static inline void insert_line(Editor* ed) { gap_insertch(&ed->lines, gapc(ed));  }

// to remove current line from lines component
static inline void remove_line(Editor* ed) { gap_removech(&ed->lines); }

// lazily update lineDelta
static void lncommit(Editor* ed) {
  if (ed->lineDelta == 0) return;
  for (u32 i = cursy(ed) + 1; i < lncount(ed); i++) {
    u32 changes = gap_getch(&ed->lines, i) + ed->lineDelta;
    gap_setch(&ed->lines, i, changes);
  }
  ed->lineDelta = 0;
}


// @ERRORS TODO
enum {
  editor_err_ok,
  editor_err_malloc_failure,
};

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


/** @VIEW **/
// curs_c must be updated before calling this method
static void editor_update_view(Editor* ed, i32 win_h, i32 win_w) {
  if (cursy(ed) > ed->view.y + win_h - SCROLL_BOUNDRY_PADDED) {
    ed->view.y += cursy(ed) - (ed->view.y + win_h - SCROLL_BOUNDRY_PADDED);
  } else if (cursy(ed) < ed->view.y + SCROLL_BOUNDRY) {
    ed->view.y = (cursy(ed) < SCROLL_BOUNDRY) ? 0 : cursy(ed) - SCROLL_BOUNDRY;
  }
  if (cursx(ed) > ed->view.x + win_w - LNO_PADDING - SCROLL_BOUNDRY_PADDED) {
    ed->view.x += cursx(ed) - (ed->view.x + win_w - LNO_PADDING - SCROLL_BOUNDRY_PADDED);    
  } else if (cursx(ed) < ed->view.x + SCROLL_BOUNDRY) {
    ed->view.x = (cursx(ed) < SCROLL_BOUNDRY) ? 0 : cursx(ed) - SCROLL_BOUNDRY;
  }
}


/** @CURS **/
static inline void update_sticky_curs(Editor* ed, u32 pos) { if (!_has(ed->cw, sticky_scroll)) ed->sticky_curs = pos; }

static void _curs_mov_vertical(Editor* ed, i32 times) {
  if (times == 0) return;
  _set(&ed->cw, sticky_scroll | commit_action);
  lncommit(ed);

  if (times > 0) {
    if (cursy(ed) == 0) goto sticky_reset;
    times = MIN(times, cursy(ed));
  } else if (times < 0) {
    if (cursy(ed) >= lncount(ed) - 1) goto sticky_reset;
    times = (cursy(ed) + -times > lncount(ed) - 1) ? -(lncount(ed) - cursy(ed) - 1) : times;
  }

  u32 target_lno = cursy(ed) - times;
  u32 target_len = lnlen(ed, target_lno);
  u32 target_pos = lnbeg(ed, target_lno) + MIN(ed->sticky_curs, target_len);
  gap_move(&ed->gap, target_pos);
  gap_move(&ed->lines, target_lno + 1);
  sticky_reset:
  _reset(&ed->cw, sticky_scroll);
}

static inline void curs_mov_up(Editor* ed, u16 times) { _curs_mov_vertical(ed, times); }
static inline void curs_mov_down(Editor* ed, u16 times) { _curs_mov_vertical(ed, -times); }

static void curs_mov_left(Editor* ed, u32 times) {
  _set(&ed->cw, commit_action);
  while (gapc(ed) > 0 && times > 0) {
    if (gap_getch(&ed->gap, gapc(ed) - 1) == '\n') {
      lncommit(ed);
      gap_left(&ed->lines, 1);
    }
    gap_left(&ed->gap, 1);
    times--;
  }
  update_sticky_curs(ed, cursx(ed));
}

static void curs_mov_right(Editor* ed, u32 times) {
  _set(&ed->cw, commit_action);
  while (gapc(ed) < GAP_LEN(&ed->gap) && times > 0) {
    if (gap_getch(&ed->gap, gapc(ed)) == '\n') {
      lncommit(ed);
      gap_right(&ed->lines, 1);
    }
    gap_right(&ed->gap, 1);
    times--;
  }
  update_sticky_curs(ed, cursx(ed));
}

static void curs_mov(Editor* ed, u32 pos) {
  if (gapc(ed) < pos) {
    curs_mov_right(ed, pos - gapc(ed));
  } else if (gapc(ed) > pos) {
    curs_mov_left(ed, gapc(ed) - pos);
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
static inline void timeline_fetch_time(Editor* ed) { clock_gettime(CLOCK_MONOTONIC, &ed->tl.time); }

static struct timeline timeline_init() {
  struct timeline tl = {0};
  tl.utop = STK_EMTY;
  tl.rtop = STK_EMTY;
  clock_gettime(CLOCK_MONOTONIC, &tl.time);
  return tl;
}

static void timeline_redo_free(struct timeline* tl) {
  for (u32 i = 0; i < UNDO_LIMIT; i++) {
    if (tl->redo[i].op != 0) {
      action_free(&tl->redo[i]);
    }
  }
  tl->rtop = STK_EMTY;
}

static void editor_update_timeline(Editor* ed, u32 ch, enum timeline_op op) {
  if (ed->tl.rtop != STK_EMTY) timeline_redo_free(&ed->tl);
  struct action* undo = ed->tl.undo;
  isize* top = &ed->tl.utop;
  if (*top == -1 || _has(ed->cw, commit_action) || elapsed_seconds(&ed->tl.time) > UNDO_EXPIRY || op != undo[*top % UNDO_LIMIT].op) {
    struct action new = action_init(gapc(ed), op);
    action_push(undo, top, new);
    _reset(&ed->cw, commit_action);
  }
  u32Vec_insert(&undo[*top % UNDO_LIMIT].frame, ch, _END(0));
  timeline_fetch_time(ed);
}

void timeline_free(struct timeline* tl) {
  timeline_redo_free(tl);
  for (u32 i = 0; i < UNDO_LIMIT; i++) {
    if (tl->undo[i].op != op_idle) {
      action_free(&tl->undo[i]);
    }
  }
}



/** @EDITOR **/
static Editor* editor_init() {
  Editor* ed = malloc(sizeof(Editor));
  if (ed == NULL) editor_err_handler(editor_err_malloc_failure, NULL);
  *ed = (Editor){0};
  ed->error = editor_err_handler;
  ed->tl = timeline_init();

  ed->gap = gap_init(1024);

  ed->lines = gap_init(1024);
  gap_insertch(&ed->lines, 0);

  _set(&ed->cw, blank);
  return ed;
}

static void editor_free(Editor** ed) {
  gap_free(&(*ed)->lines);
  gap_free(&(*ed)->gap);
  timeline_free(&(*ed)->tl);

  **ed = (Editor){0};
  free(*ed);
  *ed = NULL;
}

static void editor_insertch(Editor* ed, u32 ch) {
  if (!_has(ed->cw, undoing)) {
    editor_update_timeline(ed, ch, op_ins);
  }
  gap_insertch(&ed->gap, ch);
  ed->lineDelta++;
  if (ch == '\n') {
    insert_line(ed);
  } 
  update_sticky_curs(ed, cursx(ed));
  if (_has(ed->cw, blank)) { _reset(&ed->cw, blank); }
}

static void editor_insert_newline(Editor* ed) {
  u32 i = lnbeg(ed, cursy(ed));
  u32 curs = gapc(ed);
  editor_insertch(ed, '\n');
  while (i < curs) {
    if (gap_getch(&ed->gap, i) == '\t') {
      editor_insertch(ed, '\t');
      i++;
    } else {
      break;
    }
  }
}

static void editor_removech(Editor* ed) {
  u32 removing_ch = gap_getch(&ed->gap, gapc(ed) - 1);
  if ((u8)removing_ch == 0) return;
  if (!_has(ed->cw, undoing)) {
    editor_update_timeline(ed, removing_ch, op_del);
  }
  gap_removech(&ed->gap);
  ed->lineDelta--;
  if (removing_ch == '\n') {
    remove_line(ed);
  }
  update_sticky_curs(ed, cursx(ed));
  if (GAP_LEN(&ed->gap) == 0) { _set(&ed->cw, blank); }
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

  // reverse the vector action->frame
  u32Vec *vec = &action->frame;
  for (isize i = 0; i < vec->len / 2; i++) {
    u32 beg_val = u32Vec_get(vec, i);
    u32 end_val = u32Vec_get(vec, _END(i));
    u32Vec_set(vec, end_val, i);
    u32Vec_set(vec, beg_val, _END(i));
  }
}

void editor_undo(Editor* ed) {
  _set(&ed->cw, undoing);
  struct action* action = action_pop(ed->tl.undo, &ed->tl.utop);
  if (action == NULL) goto reset;
  timeline_invert_action(ed, action);
  action_push(ed->tl.redo, &ed->tl.rtop, *action);
  action->op = op_idle;
  reset:
    _reset(&ed->cw, undoing);
}

void editor_redo(Editor* ed) {
  _set(&ed->cw, undoing);
  struct action* action = action_pop(ed->tl.redo, &ed->tl.rtop);
  if (action == NULL) goto reset;
  timeline_invert_action(ed, action);
  action_push(ed->tl.undo, &ed->tl.utop, *action);
  action->op = op_idle;
  reset:
    _reset(&ed->cw, undoing);
}

static inline void editor_help(WINDOW* win, u16 w, u16 h) {
  char* msg = "ctrl + q to quit.";
  mvwprintw(win, CENTER(h, 0), CENTER(w, strlen(msg)), "%s", msg);
  wmove(win, 0, LNO_PADDING);
}

static void editor_draw(WINDOW* edwin, Editor* ed) {
  u16 win_h, win_w;
  getmaxyx(edwin, win_h, win_w);
  editor_update_view(ed, win_h, win_w);

  werase(edwin);
  mvwprintw(edwin, 0, 0, "%6d  ", ed->view.y + 1);

  if (_has(ed->cw, blank)) {
    editor_help(edwin, win_w, win_h);
    return;
  }

  for (u32 y = 0; y < win_h - 1  && y + ed->view.y < lncount(ed); y++) {
    u32 len = lnlen(ed, y + ed->view.y);
    mvwprintw(edwin, y, 0, "%6d ", y + ed->view.y + 1);
    for (u16 x = 0, i = 0; i + ed->view.x < len && i + LNO_PADDING < win_w; i++) {
      u8 ch = gap_getch(&ed->gap, i + lnbeg(ed, ed->view.y + y) + ed->view.x);
      mvwaddch(edwin, y, x + LNO_PADDING, ch);
      x += (ch == '\t') ? next_tab(x) : 1;
    }
    mvwprintw(edwin, y + 1, 0, "     ~");
  }
  mvwprintw(edwin, win_h - 1, 0, "delta: %ld curs: %u:%u buf_capacity: %u utop: %ld rtop: %ld",
            ed->lineDelta, cursy(ed) + 1, cursx(ed), ed->gap.capacity, ed->tl.utop, ed->tl.rtop);
  u32 y = DELTA(ed->view.y, cursy(ed));
  u32 x = get_visual_width(ed, lnbeg(ed, cursy(ed)), gapc(ed)) + LNO_PADDING - ed->view.x;
  wmove(edwin, y, x);
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
      if (ch >= 32 && ch < 127) { // ascii printable character range
        editor_insertch(ed, ch);
      } else {
        switch (ch) {
          case KEY_LEFT: curs_mov_left(ed, 1); break;
          case KEY_RIGHT: curs_mov_right(ed, 1); break;
          case KEY_UP: curs_mov_up(ed, 1); break;
          case KEY_DOWN: curs_mov_down(ed, 1); break;
          case KEY_BACKSPACE: editor_removech(ed); break;
          case '\n': editor_insert_newline(ed); break;
          case '\t': editor_insertch(ed, '\t'); break;
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
