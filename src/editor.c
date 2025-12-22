#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <ncurses.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <error.h>

#include "include/utils.h"
#include "include/itypes.h"
#include "include/gap.h"
#include "include/u32Da.h"

#define SCROLL_BOUNDRY 5
#define TAB_STOPS 4

#define FILE_NAME_MAXLEN 128
#define DEFAULT_FILE_NAME "text.txt"
#define INIT_BUFFER_SIZE 1024

#define UNDO_LIMIT 1024
#define UNDO_EXPIRY MSEC(650)
#define STK_EMTY -1

enum timeline_op { op_idle = 0, op_ins = 1, op_del = -1 };
#define DEFAULT_ACTION_FRAME_SIZ BYTE(32)

#define LNO_PADDING 8
#define PAIR_STK_SIZE 16

struct action {
  enum timeline_op op;
  u32 start;
  u32Da frame;
};

struct timeline {
  struct timespec time;
  struct action undo[UNDO_LIMIT];
  struct action redo[UNDO_LIMIT];
  isize utop;
  isize rtop;
};

enum states {
  lock_sticky = 0x1, // prevent writing to editor.curs.x for sticky cursor behaviour
  blank = 0x2, // indicates the editor text buffer is empty
  undoing = 0x4, // editor is performing an undo or redo operation
  commit_action = 0x8, // to force commit current timeline to undo
  pairing = 0x10, // to handle pairing characters
  dirty_view = 0x20, // for updating syntax higlights only when view pointer changes
  lock_modify = 0x40, // prevent the editor from inserting characters by itself
  dirty_buffer = 0x80, // contents inside buffer has to be written to file
};

typedef struct {
  enum states state;
  GapBuffer buffer;
  struct {
    u32 x;
    u32 y;
  } view; // this is visual indices. not logical
  GapBuffer lines;
  isize line_delta;
  struct timeline tl;
  u32 sticky_curs;
  u32Da pair_stack;
  FILE* fp;
  char name[FILE_NAME_MAXLEN];
} Editor;


/** @states **/
static inline void _set(enum states* st, enum states set) { *st = *st | set; }
static inline void _reset(enum states* st, enum states reset) { *st = *st & ~reset; }
static inline bool _has(enum states st, enum states has) { return (st & has) == has; }
static inline bool _has_any(enum states st, enum states has) { return (st & has) != 0; }


/** @LINES **/
// line number of cursor
static inline u32 cursy(Editor* ed) { return ed->lines.c - ((ed->lines.c) ? 1 : 0); }

// total number of lines
static inline u32 lncount(Editor* ed) { return GAP_LEN(&ed->lines); }

// logical index of cursor inside text buffer
static inline u32 cursi(Editor* ed) { return ed->buffer.c; }

// start logical index of given line lno
static inline u32 lnbeg(Editor* ed, u32 lno) {
  return gap_get(&ed->lines, lno) + (lno > cursy(ed) ? ed->line_delta : 0);
}

// logical index of line end lno
static inline u32 lnend(Editor* ed, u32 lno) {
  return (lno >= lncount(ed) - 1) ? GAP_LEN(&ed->buffer) : lnbeg(ed, lno + 1) - 1;
}

// length of a line
static inline u32 lnlen(Editor* ed, u32 lno) { return lnend(ed, lno) - lnbeg(ed, lno); }

// cursor offset relative to line start
static inline u32 cursx(Editor* ed) { return cursi(ed) - lnbeg(ed, cursy(ed)); }

// lazily update lineDelta
static void lncommit(Editor* ed) {
  if (ed->line_delta == 0) return;
  for (u32 i = cursy(ed) + 1; i < lncount(ed); i++) {
    u32 changes = gap_get(&ed->lines, i) + ed->line_delta;
    gap_set(&ed->lines, i, changes);
  }
  ed->line_delta = 0;
}


/** @CURS **/
static inline void update_sticky_curs(Editor* ed) {
  if (!_has(ed->state, lock_sticky)) {
    ed->sticky_curs = cursx(ed);
  }
}

static void _curs_mov_vertical(Editor* ed, i32 times) {
  if (times == 0) return;
  _set(&ed->state, lock_sticky | commit_action);
  u32Da_reset(&ed->pair_stack);
  lncommit(ed);

  if (times > 0) {
    if (cursy(ed) == 0) goto sticky_reset;
    times = MIN(times, cursy(ed));
  } else if (times < 0) {
    if (cursy(ed) >= lncount(ed) - 1) goto sticky_reset;
    if (cursy(ed) + -(times) > lncount(ed) - 1) {
      times = -(lncount(ed) - cursy(ed) - 1);
    }
  }

  u32 target_lno = cursy(ed) - times;
  u32 target_len = lnlen(ed, target_lno);
  u32 target_pos = lnbeg(ed, target_lno) + MIN(ed->sticky_curs, target_len);
  gap_move(&ed->buffer, target_pos);
  gap_move(&ed->lines, target_lno + 1);
  sticky_reset:
  _reset(&ed->state, lock_sticky);
}

// moves cursor up by times
static inline void curs_mov_up(Editor* ed, u16 times) { _curs_mov_vertical(ed, times); }

// moves cursor down by times
static inline void curs_mov_down(Editor* ed, u16 times) { _curs_mov_vertical(ed, -times); }

// move the cursor to the left inside buffer by times
static void curs_mov_left(Editor* ed, u32 times) {
  _set(&ed->state, commit_action);
  while (cursi(ed) > 0 && times > 0) {
    if (gap_get(&ed->buffer, cursi(ed) - 1) == '\n') {
      lncommit(ed);
      gap_left(&ed->lines, 1);
    }
    gap_left(&ed->buffer, 1);
    times--;
  }
  update_sticky_curs(ed);
}

// move the cursor to the right inside buffer by times
static void curs_mov_right(Editor* ed, u32 times) {
  _set(&ed->state, commit_action);
  while (cursi(ed) < GAP_LEN(&ed->buffer) && times > 0) {
    if (gap_get(&ed->buffer, cursi(ed)) == '\n') {
      lncommit(ed);
      gap_right(&ed->lines, 1);
    }
    gap_right(&ed->buffer, 1);
    times--;
  }
  update_sticky_curs(ed);
}

// move the cursor to desired logical index (pos)
static void curs_mov(Editor* ed, u32 pos) {
  if (cursi(ed) < pos) {
    curs_mov_right(ed, pos - cursi(ed));
  } else if (cursi(ed) > pos) {
    curs_mov_left(ed, cursi(ed) - pos);
  }
}


/** @ACTION **/
// inorder to create a action frame, the trace field should be recorded initially
static inline struct action action_init(u32 start, enum timeline_op op) {
  return (struct action) {
    .frame = u32Da_init(DEFAULT_ACTION_FRAME_SIZ),
    .op = op,
    .start = start,
  };
}

static inline void action_free(struct action* action) {
  u32Da_free(&action->frame);
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
  if (*top == -1 // The stack is empty (first action)
      || _has(ed->state, commit_action) // editor explictly instruct to commit
      || elapsed_seconds(&ed->tl.time) > UNDO_EXPIRY // time expired since last action
      || op != undo[*top % UNDO_LIMIT].op // current operation is different from previous
  ) {
    struct action new = action_init(cursi(ed), op);
    action_push(undo, top, new);
    _reset(&ed->state, commit_action);
  }
  u32Da_insert(&undo[*top % UNDO_LIMIT].frame, ch, _END(0));
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


bool _is_open_pair(u32 ch) {
  switch (ch) {
    case '(' :
    case '[' :
    case '{' :
    case '\'':
    case '"' :
    case '`' :
      return true;
    default : return false;
  }
}

bool _is_closing_pair(u32 ch) {
  switch (ch) {
    case ')' :
    case ']' :
    case '}' :
    case '\'':
    case '"' :
    case '`' :
      return true;
    default : return false;
  }
}

u32 get_pair(u32 ch) {
  switch (ch) {
    case '(': return ')';
    case '[': return ']';
    case '{': return '}';
    case ')': return '(';
    case ']': return '[';
    case '}': return '{';
    default: return ch;
  }
}

static inline bool is_quote(u32 ch) { return ch == '\'' || ch == '"' || ch == '`'; }

// stack operations for Editor.pair_stack
static inline void push_pair(Editor* ed, u32 ch) { u32Da_insert(&ed->pair_stack, ch, _END(0)); }
static inline u32 peek_pair(Editor* ed) { return u32Da_get(&ed->pair_stack, _END(0)); }
static inline u32 pop_pair(Editor* ed) { return u32Da_remove(&ed->pair_stack, _END(0)); }
static inline bool _is_empty_pair_stk(Editor* ed) { return ed->pair_stack.len == 0; }

static void editor_insert(Editor* ed, u32 new_ch) {
  u32 prev_ch = gap_get(&ed->buffer, cursi(ed) - 1);
  u32 curr_ch = gap_get(&ed->buffer, cursi(ed));
  // skip closing pair if exists
  if (!_has_any(ed->state, pairing | undoing) && _is_closing_pair(new_ch) && !_is_empty_pair_stk(ed)) {
    if (curr_ch == new_ch && get_pair(new_ch) == peek_pair(ed)) {
      curs_mov_right(ed, 1);
      pop_pair(ed);
      return;
    }
  }
  if (!_has_any(ed->state, undoing | lock_modify)) {
    editor_update_timeline(ed, new_ch, op_ins);
  }
  // insertion of newch into buffer
  gap_insert(&ed->buffer, new_ch);
  ed->line_delta++;
  if (new_ch == '\n') { // handling lines
    gap_insert(&ed->lines, cursi(ed));
  } 
  update_sticky_curs(ed);
  // update blank state on insertion
  if (_has(ed->state, blank)) {
    u32Da_reset(&ed->pair_stack);
    _reset(&ed->state, blank);
  }

  // pair insertion (if any)
  if (_is_open_pair(new_ch) && !_has_any(ed->state, undoing | pairing | lock_modify)) {
    if (is_quote(new_ch) && isalpha(prev_ch)) { // refuse to pair quotes followed by alphabet
      return;
    }
    _set(&ed->state, pairing);
    push_pair(ed, new_ch);
    editor_insert(ed, get_pair(new_ch));
    curs_mov_left(ed, 1);
    _reset(&ed->state, pairing);
  }

  if (!_has_any(ed->state, dirty_buffer | lock_modify)) {
    _set(&ed->state, dirty_buffer);
  }
}

static void indent_from_prevln(Editor* ed) {
  u32 i = lnbeg(ed, cursy(ed) - 1);
  while (i < cursi(ed)) { // indenting current line with same level as previous line
    if (gap_get(&ed->buffer, i) == '\t') {
      editor_insert(ed, '\t');
      i++;
    } else {
      break;
    }
  }
}

static void editor_insert_newline(Editor* ed) {
  u32 prev_ch = gap_get(&ed->buffer, cursi(ed) - 1);
  u32 curr_ch = gap_get(&ed->buffer, cursi(ed));
  editor_insert(ed, '\n');
  indent_from_prevln(ed);
  // automatic line insertion for auto pairs
  if (_is_open_pair(prev_ch) && curr_ch == get_pair(prev_ch)) {
    editor_insert(ed, '\n'); // (\n\n)
    indent_from_prevln(ed); // \t\t ... (\n\t\t\n)
    curs_mov_up(ed, 1); // (\n^\n)
    editor_insert(ed, '\t'); // (\n\t\n)
  } else if (!is_quote(prev_ch) && _is_open_pair(prev_ch)) {
    editor_insert(ed, '\t');
  } else if (prev_ch == ':') { // for python like langs
    editor_insert(ed, '\t');
  }
}

static void editor_removel(Editor* ed) {
  u32 removing_ch = gap_get(&ed->buffer, cursi(ed) - 1);
  if ((u8)removing_ch == 0) return;

  u8 removal_weight = 1;
  if (_is_open_pair(removing_ch) && gap_get(&ed->buffer, cursi(ed)) == get_pair(removing_ch)) {
    curs_mov_right(ed, 1);
    removal_weight = 2;
  }

  while (removal_weight > 0) {
    u32 ch = gap_get(&ed->buffer, cursi(ed) - 1);
    if (!_has(ed->state, undoing)) {
      editor_update_timeline(ed, ch, op_del);
    }
    gap_remove(&ed->buffer);
    ed->line_delta--;
    removal_weight--;
  }
  if (removing_ch == '\n') {
    gap_remove(&ed->lines);
  }
  update_sticky_curs(ed);
  if (GAP_LEN(&ed->buffer) == 0) { _set(&ed->state, blank); }
  _set(&ed->state, dirty_buffer);
}

static void editor_remover(Editor* ed) {
  curs_mov_right(ed, 1);
  editor_removel(ed);
}

static void timeline_invert_action(Editor* ed, struct action* action) {
  if (action->op == op_idle) return;
  
  action->start += action->op * action->frame.len;
  curs_mov(ed, action->start);
  if (action->op == op_ins) {
    for (isize i = 0; i < action->frame.len; i++) {
      editor_removel(ed);
    }
  } else if (action->op == op_del) {
    for (isize i = 0; i < action->frame.len; i++) {
      editor_insert(ed, u32Da_get(&action->frame, _END(i)));
    }
  }
  action->op *= -1;

  // reverse the vector action->frame
  u32Da *vec = &action->frame;
  for (isize i = 0; i < vec->len / 2; i++) {
    u32 beg_val = u32Da_get(vec, i);
    u32 end_val = u32Da_get(vec, _END(i));
    u32Da_set(vec, end_val, i);
    u32Da_set(vec, beg_val, _END(i));
  }
}

void editor_undo(Editor* ed) {
  _set(&ed->state, undoing);
  struct action* action = action_pop(ed->tl.undo, &ed->tl.utop);
  if (action == NULL) goto reset;
  timeline_invert_action(ed, action);
  action_push(ed->tl.redo, &ed->tl.rtop, *action);
  action->op = op_idle;
  reset:
    _reset(&ed->state, undoing);
}

void editor_redo(Editor* ed) {
  _set(&ed->state, undoing);
  struct action* action = action_pop(ed->tl.redo, &ed->tl.rtop);
  if (action == NULL) goto reset;
  timeline_invert_action(ed, action);
  action_push(ed->tl.undo, &ed->tl.utop, *action);
  action->op = op_idle;
  reset:
    _reset(&ed->state, undoing);
}

static inline void editor_display_help(Editor* ed, WINDOW* win, u16 w, u16 h) {
  char* msg = "ctrl + q to quit";
  mvwprintw(win, CENTER(h, 0), CENTER(w, strlen(msg)), "%s\n%s", msg, ed->name);
  wmove(win, 0, LNO_PADDING);
}

/** @VIEW **/
// return distance to next tabstop from pos
static inline u8 tabstop_distance(u32 pos) { return TAB_STOPS - (pos % TAB_STOPS); }

// returns visual length(length with tab character) from start to (i < end)
static u32 vlen(Editor* ed, u32 start, u32 end) {
  u32 width = 0;
  for (u32 i = start; i < end; i++) {
    if (gap_get(&ed->buffer, i) == '\t') {
      width += tabstop_distance(width);
    } else {
      width++;
    }
  }
  return width;
}

static void update_view(Editor* ed, u16 win_h, u16 win_w) {
  // updating view.y
  u32 vy = ed->view.y, vx = ed->view.x;
  const u32 scroll_down_threshold = vy + win_h - SCROLL_BOUNDRY;
  if (cursy(ed) > scroll_down_threshold) {
    vy += cursy(ed) - scroll_down_threshold; // scroll down
  } else if (cursy(ed) < vy + SCROLL_BOUNDRY) {
    if (cursy(ed) < SCROLL_BOUNDRY) { // scroll up
      vy = 0;
    } else {
      vy = cursy(ed) - SCROLL_BOUNDRY;
    }
  }

  // updating view.x
  const u16 content_w = win_w - LNO_PADDING;
  const u32 visual_cursx = vlen(ed, lnbeg(ed, cursy(ed)), cursi(ed)); // find visual cursx position
  const u32 scroll_right_threshold = vx + content_w - SCROLL_BOUNDRY;

  if (visual_cursx >= scroll_right_threshold) {
    vx = visual_cursx - (content_w - SCROLL_BOUNDRY); // right
  } else if (visual_cursx < vx + SCROLL_BOUNDRY) {
    if (visual_cursx < SCROLL_BOUNDRY) { // left
     vx = 0;
    } else {
      vx = visual_cursx - SCROLL_BOUNDRY;
    }
  }
  if (vx != ed->view.x || vy != ed->view.y) {
    _set(&ed->state, dirty_view);
  }
  ed->view.y = vy;
  ed->view.x = vx;
}


// @FILE_HANDLING
static void sync_file_content(Editor* ed) {
  i8 ch;
  while ((ch = fgetc(ed->fp)) != EOF) {
    editor_insert(ed, ch);
  }
  curs_mov(ed, 0);
}

static void open_from_file(Editor* ed, char* filepath) {
  _set(&ed->state, lock_modify);
  struct stat st;
  if (stat(filepath, &st) == 0) { // obtains file stat
    if (S_ISDIR(st.st_mode)) { // if it is a directory

      // directory TODO
      return;
    } else { // reading from existing file
      strncpy(ed->name, filepath, FILE_NAME_MAXLEN);
      ed->fp = fopen(filepath, "r+");
      if (ed->fp == NULL) {
        error(EXIT_FAILURE, errno, "%s(ed, %s)", __FUNCTION__, filepath);
      }
      sync_file_content(ed);
    }
  } else { // file doesn't exist, but saving given filename to create one later
    strncpy(ed->name, filepath, FILE_NAME_MAXLEN);
  }
  _reset(&ed->state, lock_modify);
}

static void write_to_file(Editor* ed) {
  if (_has(ed->state, dirty_buffer)) {
    u32 len = GAP_LEN(&ed->buffer), i;
    char *buf = malloc(sizeof(char) * len);
    for (i = 0; i < len; i++) {
      buf[i] = (char)gap_get(&ed->buffer, i);
    }
    if (ed->fp == NULL) {
      if (*ed->name == '\0') { // obtain filename from user
        strncpy(ed->name, DEFAULT_FILE_NAME, FILE_NAME_MAXLEN);
      }
      ed->fp = fopen(ed->name, "w+");
      if (ed->fp == NULL) {
        free(buf);
        error(EXIT_FAILURE, errno, "%s", __FUNCTION__);
      }
    }
    rewind(ed->fp);
    fwrite(buf, sizeof(char), len, ed->fp);
    fflush(ed->fp);
    if (ftruncate(fileno(ed->fp), ftell(ed->fp)) == -1) {
      error(0, errno, "ftruncate");
    }
    free(buf);
    _reset(&ed->state, dirty_buffer);
  }
}

static Editor* editor_init(char* filepath) {
  Editor* ed = malloc(sizeof(Editor));
  if (ed == NULL) {
    error(EXIT_FAILURE, errno, "malloc");
  }
  *ed = (Editor){0};
  ed->tl = timeline_init();
  ed->buffer = gap_init(INIT_BUFFER_SIZE);
  ed->lines = gap_init(INIT_BUFFER_SIZE);
  gap_insert(&ed->lines, 0);
  ed->pair_stack = u32Da_init(PAIR_STK_SIZE);

  _set(&ed->state, blank);
  if (filepath != NULL) {
    open_from_file(ed, filepath);
  }

  return ed;
}

static void editor_free(Editor** ed) {
  gap_free(&(*ed)->lines);
  gap_free(&(*ed)->buffer);
  timeline_free(&(*ed)->tl);
  u32Da_free(&(*ed)->pair_stack);
  if ((*ed)->fp != NULL) {
    fclose((*ed)->fp);
  }
  **ed = (Editor){0};
  free(*ed);
  *ed = NULL;
}

static void editor_exit(Editor* ed) {
  if (!_has(ed->state, dirty_buffer)) {
    exit(EXIT_SUCCESS);
  }
}

static void editor_draw(WINDOW* edwin, Editor* ed) {
  u16 win_h, win_w;
  getmaxyx(edwin, win_h, win_w);
  werase(edwin);
  mvwprintw(edwin, 0, 0, "%6d ", ed->view.y + 1);

  if (_has(ed->state, blank)) {
    editor_display_help(ed, edwin, win_w, win_h);
    return;
  }

  update_view(ed, win_h, win_w);
  const u16 content_w = win_w - LNO_PADDING;
  const u32 visual_cursx = vlen(ed, lnbeg(ed, cursy(ed)), cursi(ed)); // find visual cursx position
   u32 vy;

  for (vy = 0; vy < win_h && vy + ed->view.y < lncount(ed); vy++) {
    u32 line_idx = vy + ed->view.y;
    u32 start = lnbeg(ed, line_idx);
    u32 len = lnlen(ed, line_idx);
    
    mvwprintw(edwin, vy, 0, "%6d ", line_idx + 1);

    for (u32 i = 0, vx = 0; i < len; i++) {
      u32 ch = gap_get(&ed->buffer, start + i);
      u16 char_width = (ch == '\t') ? tabstop_distance(vx) : 1;

      if (vx + char_width > ed->view.x && vx < ed->view.x + content_w) {
        u32 screen_x = vx + LNO_PADDING - ed->view.x;
        if (ch == '\t') {
          for (u32 k = 0; k < char_width; k++) {
             if (vx + k >= ed->view.x && screen_x + k < win_w) {
                 mvwaddch(edwin, vy, screen_x + k, ' ');
             }
          }
        } else if (screen_x < win_w) {
          mvwaddch(edwin, vy, screen_x, ch);
        }
      }
      vx += char_width;
    }
  }
  mvwprintw(edwin, vy, 0, "      ~");

  u16 cy = DELTA(ed->view.y, cursy(ed));
  u16 cx = visual_cursx - ed->view.x + LNO_PADDING;
  
  wmove(edwin, cy, cx);
  _reset(&ed->state, dirty_view);
}
