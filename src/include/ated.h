#pragma once

// realloc step for lines.map table
#define ALLOC_STEP 1024

// find min value
#define MIN(a,b) ((a) < (b) ? (a) : (b))

// to find the center of offseted length
#define CENTER(length, offset) ((((length) - (offset)) / 2))

// to make ctrl + ; key maps more readable
#define CTRL(x) ((x) & 0x1F)

// all the modifier types are defined here
enum modifiers {
  _lock_cursx = 0x8000, // prevent writing to editor.curs.x; method: editor_update_cursx()
};

