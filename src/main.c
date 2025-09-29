#include <curses.h>
#include <stdlib.h>
#include <ncurses.h>
#include <stdint.h>
#include <string.h>

// [start]abcd[c]_______________[ce]efg[end]
struct gap_buf {
  char* start;  // pointer to the start of buffer
  char* end;  // pointer to end of the buffer
  char* c;  // start of gap
  char* ce;  // end of gap
  char* marker;  // characters are displayed from here (for vertical scrolling)
  size_t capacity;  // total capacity of gap buffer. can grow
};

// initialize gap buffer of capacity = size
struct gap_buf gap_init(size_t size) {
  struct gap_buf gap;
  gap.capacity = size;
  gap.start = malloc(sizeof(char) * size);
  if (!gap.start) {
    perror("failed to initialize gap buffer.");
  }
  memset(gap.start, 0, size);
  gap.end = gap.start + size - 1;
  gap.c = gap.start;
  gap.ce = gap.end;
  gap.marker = gap.start;
  return gap;
}

// frees gap buffer
void gap_free(struct gap_buf* gap) {
  free(gap->start);
  gap->start = NULL;
  gap->end = NULL;
  gap->c = NULL;
  gap->ce = NULL;
  gap->capacity = 0;
}

// grow operation of gap buffer
void gap_grow(struct gap_buf* gap) {
  // after realloc, memory addresses will change, hence the need to
  // recalculate the pointers of gap buffer. Offsets are calculated
  // to correctly setup c and ce pointers after realloc.
  uint32_t ce_offset = gap->end - gap->ce;
  uint32_t c_offset = gap->c - gap->start;
  uint32_t marker_offset = 0;
  if (gap->marker) {  // if marker is initialized
    marker_offset = gap->marker - gap->start;
  }

  gap->capacity *= 2;  // capacity is doubled
  gap->start = realloc(gap->start, gap->capacity);
  if (!gap->start) {
    perror("realloc failure");
  }

  gap->end = gap->start + gap->capacity - 1;

  gap->ce = gap->end - (gap->capacity / 2) - ce_offset;
  // this position is where ce pointed before realloc
  if (ce_offset > 0) {  // copy characters after ce to the end if ce < end
    for (int i = 0; i <= ce_offset; i++) {
      *(gap->end - ce_offset + i) = *(gap->ce + i); 
    }
  }
  gap->marker = gap->start + marker_offset;
  gap->ce = gap->end - ce_offset;  // this is the correct ce position
  gap->c = gap->start + c_offset;
}

// insert operation of gap buffer.
void gap_insert_ch(struct gap_buf* gap, char ch) {
  if (gap->c >= gap->ce) {
    gap_grow(gap);
  }
  *gap->c++ = ch;
}

// remove operation
void gap_remove_ch(struct gap_buf* gap) {
  if (gap->c > gap->start) {
    gap->c--;
  }
}

// moves the gap max `n_ch` times to the left
void gap_left(struct gap_buf* gap, int n_ch) {
  int offset = gap->c - gap->start;
  if (offset > 0) {
    for (int i = 0; offset > 0 && i < n_ch; i++) {
      *gap->ce = *(gap->c - 1);
      gap->ce--;
      gap->c--;
      offset--;
    }
  }
}

// moves the gap max `n_ch` times to the right
void gap_right(struct gap_buf* gap, int n_ch) {
  int offset = gap->end - gap->ce;
  if (offset > 0) {
    for (int i = 0; i < n_ch && offset > 0; i++) {
      *gap->c = *(gap->ce + 1);
      gap->ce++;
      gap->c++;
      offset--;
    }
  }
}

// handle overflows(horizontal and vertical)
// up and down movements

// returns the line number of where *end is
uint32_t get_line_no(char *ptr, const char* end) {
  uint32_t count = 1;
  while (ptr <= end) {
    if (*ptr == '\n') {
      count++;
    }
    ptr++;
  }  
  return count;
}

// returns the total number of lines in the gap buffer.
uint32_t get_line_count(const struct gap_buf* gap) {
  char* buf = gap->start;
  uint32_t count = 1;
  while (buf < gap->c) {
    if (*buf == '\n') {
      count++;
    }
    buf++;
  }
  if (gap->ce != gap->end) {
    buf = gap->ce + 1;
    while (buf < gap->end) {
      if (*buf == '\n') {
        count++;
      }
      buf++;
    }
  }
  return count;
}

void marker_next(struct gap_buf* gap) {
  while (gap->marker <= gap->end && *gap->marker != '\n') gap->marker++;
  gap->marker++;
}

// todo
void marker_prev(struct gap_buf* gap) {
  while (gap->marker >= gap->start && *gap->marker != '\n') gap->marker--;
  if (gap->marker != gap->start) gap->marker++;
}

void draw_text(struct gap_buf* gap) {
  const uint32_t line_count = get_line_count(gap);
  const uint32_t curs_line_no = get_line_no(gap->start, gap->c);
  uint32_t marker_line_no = get_line_no(gap->start, gap->marker);

  while (curs_line_no - marker_line_no > LINES - 4) {
    marker_next(gap);
    marker_line_no = get_line_no(gap->start, gap->marker);
  }

  char* pencil = gap->marker;

  erase();
  uint32_t curs_x = 0, curs_y = 0, x = 0, y = 0;

  while (pencil <= gap->end && y < LINES) {
    if (pencil == gap->c) {
      curs_x = x;
      curs_y = y;
      if (gap->ce == gap->end) {
        break; 
      }
      pencil = gap->ce + 1;
    }
    mvaddch(y, x++, *pencil);
    if (*pencil == '\n') {
      y++;
      x = 0;
    }
    pencil++;
  }
  move(curs_y, curs_x);
}

void gap_visualize(struct gap_buf gap);

int main() {
  struct gap_buf gap = gap_init(1);
  initscr();
  noecho();
  curs_set(1);
  keypad(stdscr, TRUE);
  cbreak();
  
  refresh();
  
  int ch;
  while ((ch = getch()) != 27) {
    switch (ch) {
      case KEY_LEFT:
        gap_left(&gap, 1);
        break;
      case KEY_RIGHT:
        gap_right(&gap, 1);
        break;
      case KEY_UP:
        break;
      case KEY_DOWN:
        break;
      case KEY_BACKSPACE:
        gap_remove_ch(&gap);
        break;
      case KEY_ENTER:
        gap_insert_ch(&gap, '\n');
        break;
      default:
        gap_insert_ch(&gap, (char)ch);
        break;
    }
    draw_text(&gap);
    refresh();
  }
  
  endwin();
  gap_free(&gap);
  return 0;
}





void gap_visualize(struct gap_buf gap) {
    printf("capty:\t%lu\nstart:\t%p\nc:\t%p\nce:\t%p\nend:\t%p\n", gap.capacity, gap.start, gap.c,gap.ce,gap.end);
    char* buf = gap.start;
    printf("[s]");
    
    while (buf <= gap.end) {
        if (buf == gap.c) {
            printf("[");
            while (buf < gap.ce) {
                printf("_");
                buf++;
            }
            printf("]");
            buf++;
            continue;
        }
        
        printf("%c", *buf ? *buf : '.');
        buf++;
    }
    
    printf("[E]\n");
} 
