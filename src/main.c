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
  u32 begin;
  u32 end;
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
  u32 y; // points to buffer index where the view should begin
  u32 x;
  lc_t lno; // line number of view ptr in the buffer
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
  _mark_eol = 0x4000,   // mark end of line when a '\n' character is encountered
};

// used to set modifier. you can input multiple modifier using the OR operator
static inline void editor_set_mods(u16* mods, u16 new_mods) { *mods = *mods | new_mods; }

// used to reset mods. you can input multiple mods using the OR operator
static inline void editor_reset_mods(u16* mods, u16 new_mods) { *mods = *mods & ( new_mods ^ 0xffff); }

// used to check modifiers
// static inline bool editor_has_mods(u16 mods, u16 new_mods) { return mods & new_mods; }

// updates editor.curs.x to pos if the lock_cursx state is set to zero.
static inline void editor_update_cursx(Editor* ed, u16 pos) { if (!(ed->mods & _lock_cursx)) ed->curs.x = pos; }

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

// this method will gracefully abstract the GapBuffer into lines and saves
// it in the line component of editor.
// void editor_linefy_buf(Editor* ed, GapBuffer* gap) {
//   if (GAPBUF_LEN(gap) == 0) { // if the gap_buffer is empty
//     ed->lines.count = ed->view.lno = ed->curs.lno = 0;
//     return;
//   }
//   lc_t lc = 0; // used to track Line Count. incremented on each '\n' character.
//   bool is_line_start = true; // a state for recording start and end positions of a line. 
//   for (u32 i = 0; i <= gap->end; i++) {
//     if (is_line_start) { // marks current logical index as begining of current line.
//       ed->lines.map[lc].begin = GAPBUF_GET_LOGICAL_INDEX(gap, i);
//       is_line_start = false;
//     }
//     if (i == ed->view.y) ed->view.lno = lc;
//     if (i == gap->c) { // we have to skip the gap after saving cursor line number.
//       ed->curs.lno = lc;
//       ed->curs.y = lc - ed->view.lno;
//       // conditionally updates curs.x as offset from beginning of current line.
//       editor_update_cursx(ed, GAPBUF_GET_LOGICAL_INDEX(gap, i) - ed->lines.map[lc].begin);
//       i = (gap->ce < gap->end) ? gap->ce : gap->end;
//       continue;
//     }
    
//     if (gap->start[i] == '\n') {
//       ed->lines.map[lc].end = GAPBUF_GET_LOGICAL_INDEX(gap, i); // saves end position
//       is_line_start = true; // to mark next character as start of next line.
//       lc++;
//       if (lc >= ed->lines.capacity) { // handle line map overflows.
//         ed->lines.capacity += ALLOC_FAC;
//         ed->lines.map = realloc(ed->lines.map, sizeof(struct span) * ed->lines.capacity);
//         if (!ed->lines.map) {
//           perror("failed to do realloc for lines.map");
//           exit(-1);
//         }
//       }
//     }
//   }
//   ed->lines.count = lc;
//   if (!is_line_start) { // if there is no '\n' at the end
//     ed->lines.map[lc].end = GAPBUF_LEN(gap) - 1;
//     ed->lines.count = lc + 1;
//   }
// }

// todo: implement this method with incremental line updation
void editor_linefy_buf(Editor* ed, GapBuffer* gap) {

}

// updates the view component of editor
void editor_update_view(Editor* ed, GapBuffer* gap) {
  while (ed->curs.lno - ed->view.lno > LINES - 1) {
    ed->view.y = ed->lines.map[ed->view.lno + 1].begin;
    editor_linefy_buf(ed, gap);
  }
  while (ed->curs.lno - ed->view.lno < 0) {
    ed->view.y = ed->lines.map[ed->view.lno - 1].begin;
    editor_linefy_buf(ed, gap);
  }
}

// renders the gap buffer as desired.
void editor_draw(Editor* ed, GapBuffer* gap) {
  struct span* ln_map = ed->lines.map;
  erase();
  for (u32 ln = 0; ln < LINES  && ln < ed->lines.count; ln++) {
    for (u32 i = ln_map[ln + ed->view.lno].begin; i <= ln_map[ln + ed->view.lno].end; i++) {
      addch(gap_getch(gap, i));
    }
  }
  move(ed->curs.y, gap->c - ed->lines.map[ed->curs.lno].begin);
}

// to handle implicit editor modifiers like preserving cursx position
// when doing a vertical scroll.
void editor_handle_implicit_mods(Editor* ed, u32 ch) {
  u16 auto_reset = _mark_eol | _lock_cursx;
  switch (ch) {
    case KEY_UP:
    case KEY_DOWN: editor_set_mods(&ed->mods, _lock_cursx); return;
    case KEY_ENTER: editor_set_mods(&ed->mods, _mark_eol); return;
  }
  editor_reset_mods(&ed->mods, auto_reset);
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
