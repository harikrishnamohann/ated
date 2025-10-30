#include <ncurses.h>
#include "gap_buffer.c"
#include <stdbool.h>
#include "include/editor.h"
#include "include/rust_itypes.h"

static inline void _set_mods(Editor* ed, u16 new_mods) { ed->mods = ed->mods | new_mods; }
static inline void _reset_mods(Editor* ed, u16 new_mods) { ed->mods = ed->mods & ( new_mods ^ 0xffff); }
static inline bool _has_mods(Editor* ed, u16 mods) { return (ed->mods & mods) == mods; }
static inline void _update_cursx(Editor* ed, u16 pos) { if (!_has_mods(ed, _lock_cursx)) ed->curs.x = pos; }

// the following functions return logical indices from lines.map.
u32 static inline _lnstart(Editor* ed, lc_t lno) { return ed->lines.map[lno]; }
u32 static inline _viewpos(Editor* ed, lc_t ln_offset) { return ed->lines.map[ln_offset + ed->view.y]; }
u32 static inline _curspos(Editor* ed, lc_t ln_offset) { return ed->lines.map[ln_offset + ed->curs.y]; }

#define LN_EMPTY U32_MAX
// returns logical end position of given lno from line component of editor.
// returns LN_EMPTY for empty lines
static u32 _lnend(Editor* ed, GapBuffer* gap, lc_t lno) {
  u32 i = _lnstart(ed, lno);
  u32 len = GAPBUF_LEN(gap);
  if (i >= len) return LN_EMPTY;
  while (i < len - 1 && gap_getch(gap, i) != '\n') i++;
  return i;
}

static void _increment_lc(struct lines_c* ln) {
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

// curs_c must be updated before calling this method
static void _update_view(Editor* ed, GapBuffer* gap, u16 win_h, u16 win_w) {
  while (ed->curs.y - ed->view.y > win_h - SCROLL_MARGIN - 1) { // down scrolling
    ed->view.offset = ed->lines.map[++ed->view.y];
  }
  while (ed->view.y > 0 && ed->curs.y < ed->view.y + SCROLL_MARGIN) { // upwards
    ed->view.offset = ed->lines.map[--ed->view.y];
  }
}

static void _handle_implicit_mods(Editor* ed, u32 ch) {
  switch (ch) {
    case KEY_UP:
    case KEY_DOWN: _set_mods(ed, _lock_cursx); return;
  }
  _reset_mods(ed, _lock_cursx);
}

// the following 4 operations expects curs_c before updation.

static inline void _increment_next_lines(Editor* ed) { for (lc_t i = ed->curs.y + 1; i < ed->lines.count; i++) ed->lines.map[i]++; }
static inline void _decrement_next_lines(Editor* ed) { for (lc_t i = ed->curs.y + 1; i < ed->lines.count; i++) ed->lines.map[i]--; }

static void _add_newline(Editor* ed, u32 logical_pos) {
  _increment_lc(&ed->lines);
  for (lc_t i = ed->lines.count - 1; i > ed->curs.y + 1; i--) {
    ed->lines.map[i] = ed->lines.map[i - 1]; // shifting right from curs_lno + 1 pos
  }
  // curs_lno + 1 position should be unoccupied now
  ed->lines.map[ed->curs.y + 1] = logical_pos; // new position is inserted there
}

static void _remove_newline(Editor* ed) {
  for (lc_t i = ed->curs.y + 1; i < ed->lines.count; i++) {
    ed->lines.map[i - 1] = ed->lines.map[i];
  }
  ed->lines.count--;
}

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

void editor_insertch(Editor* ed, GapBuffer* gap, u32 ch) {
  gap_insertch(gap, ch);
  _increment_next_lines(ed);
  if (ch == '\n') {
    _add_newline(ed, gap->c);
    ed->curs.y++;
  }
  _update_cursx(ed, gap->c - _curspos(ed, 0));
}

void editor_removech(Editor* ed, GapBuffer* gap) {
  u8 removing_ch = gap_getch(gap, gap->c - 1);
  if (!removing_ch) return;
  _decrement_next_lines(ed);
  gap_removech(gap);
  if (removing_ch == '\n') {
    _remove_newline(ed);
    ed->curs.y--;
  }
  _update_cursx(ed, gap->c - _curspos(ed, 0));
}

void editor_curs_mov_left(Editor* ed, GapBuffer* gap) {
  if (gap->c > 0) {
    u8 prev_ch = gap->start[gap->c - 1];
    if (prev_ch == '\n') {
      ed->curs.y--;
    }
    gap_left(gap);
    _update_cursx(ed, gap->c - _curspos(ed, 0));
  }
}

void editor_curs_mov_right(Editor* ed, GapBuffer* gap) {
  u8 next_ch = gap_getch(gap, gap->c);
  if (next_ch != 0) {
    if (next_ch == '\n') ed->curs.y++;
    gap_right(gap);
    _update_cursx(ed, gap->c - _curspos(ed, 0));
  }
}

void editor_curs_mov_up(Editor* ed, GapBuffer* gap, u16 steps) {
  if (ed->curs.y == 0) return;
  steps = (steps > ed->curs.y) ? ed->curs.y : steps;
  u16 target_lno = ed->curs.y - steps;
  u16 target_ln_len = _lnend(ed,gap, target_lno) - _lnstart(ed, target_lno);
  u16 target_pos = _lnstart(ed, target_lno) + MIN(ed->curs.x, target_ln_len);
  while (gap->c > target_pos) gap_left(gap);
  ed->curs.y -= steps;
}

void editor_curs_mov_down(Editor* ed, GapBuffer* gap, u16 steps) {
  if (ed->curs.y >= ed->lines.count - 1) return;
  steps = (ed->curs.y + steps > ed->lines.count - 1) ? ed->lines.count - ed->curs.y - 1 : steps;
  u16 target_lno = ed->curs.y + steps;
  u32 ln_len = _lnend(ed, gap, target_lno);
  if (ln_len == LN_EMPTY) return;
  u16 target_ln_len = ln_len - _lnstart(ed, target_lno);
  u16 target_pos = _lnstart(ed, target_lno) + MIN(ed->curs.x, target_ln_len);
  while (gap->c < target_pos) gap_right(gap);
  ed->curs.y += steps;
}

void editor_draw(WINDOW* edwin, Editor* ed, GapBuffer* gap) {
  u16 win_h, win_w;
  getmaxyx(edwin, win_h, win_w);

  _update_view(ed, gap, win_h, win_w);

  werase(edwin);
  mvwvline(edwin, 0, 0, ACS_VLINE, win_h);
  mvwvline(edwin, 0, win_w - 1, ACS_VLINE, win_h);
  mvwprintw(edwin, 0, 1, "%5d ", ed->view.y + 1);

  for (u32 ln = 0; ln < win_h  && ln + ed->view.y < ed->lines.count; ln++) {
    u32 lnend = _lnend(ed, gap, ln + ed->view.y);
    if (lnend != LN_EMPTY) {
      mvwprintw(edwin, ln, 1, "%5d ", ln + ed->view.y + 1);

      for (u16 x = 0; x + _viewpos(ed, ln) <= lnend && x + LNO_PADDING < win_w - 1; x++) {
        u8 ch = gap_getch(gap, x + _viewpos(ed, ln));
        if (ch && ch != '\n') mvwaddch(edwin, ln, x + LNO_PADDING, ch);
      }

      mvwprintw(edwin, ln + 1, 1, "%5d ", ln + ed->view.y + 2);
    }
  }
  wmove(edwin, ed->curs.y - ed->view.y, MIN(win_w - 1 ,gap->c - _curspos(ed, 0) + LNO_PADDING));
}

void editor_process(Editor* ed, WINDOW* edwin) {
  GapBuffer gap = gap_init(ALLOC_STEP);

  u32 ch;
  editor_draw(edwin, ed, &gap);
  wrefresh(edwin);
  while ((ch = wgetch(edwin)) != CTRL('q')) {
    if (ch != ERR) {
      _handle_implicit_mods(ed, ch);
      switch (ch) {
        case KEY_LEFT: editor_curs_mov_left(ed, &gap); break;
        case KEY_RIGHT: editor_curs_mov_right(ed, &gap); break;
        case KEY_UP: editor_curs_mov_up(ed, &gap, 1); break;
        case KEY_DOWN: editor_curs_mov_down(ed, &gap, 1); break;
        case KEY_BACKSPACE: editor_removech(ed, &gap); break;
        case KEY_ENTER: editor_insertch(ed, &gap, '\n'); break;
        default: editor_insertch(ed, &gap, ch); break;
      }
      editor_draw(edwin, ed, &gap);
    }
    wrefresh(edwin);
  }
  
  gap_free(&gap);
}

#undef lc_t
#undef SCROLL_MARGIN
#undef LN_EMPTY

/*
TODO
 - up and down movements [v]
 - snap scrolling [v]
 - split editor operations into component level [v]
 - make editior updation incremental [v] NOTE I can do this using gap buffer
 - make Editor window attachable [v]
 - handle horizontal text overflows [_]
 - make a header file and compile this separately once i am done with this[_]
*/
