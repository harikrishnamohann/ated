#pragma once

#ifndef __EDITOR__
#define __EDITOR__
#include "rust_itypes.h"
#include "ated.h"

// lc_t will specify the maximum number of lines possible.
#define lc_t u16
#define SCROLL_MARGIN 3
#define LNO_PADDING 8


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

enum editor_modifiers {
  _lock_cursx = 0x8000, // prevent writing to editor.curs.x; method: editor_update_cursx()
};

#endif
