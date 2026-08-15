#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <termios.h>
#include <sys/ioctl.h>

#include "clem/itypes.h"
#define ANSI_24BIT_COLORS
#define ANSI_STD_DECALS
#include "clem/utils.h"
#include "clem/err.c"
#include "clem/da.h"

#define LITLEN(s) (sizeof("" s) - 1)

#define TABSTOP 4

/* Escape sequences */
#define CLRSCR "\x1b[2J"
#define CLRLN "\x1b[K"
#define GETCURS "\x1b[6n"
#define CURSOFF "\x1b[?25l"
#define CURSON "\x1b[?25h"


enum EditorKeys {
    KEY_LEFT = 1000,
    KEY_RIGHT,
    KEY_DOWN,
    KEY_UP,
    KEY_PAGE_UP,
    KEY_PAGE_DOWN,
    KEY_HOME,
    KEY_END,
    KEY_DEL,
};
// data
typedef struct line {
    char *content;
    char *render;
    usize rsize;
    usize size;
} EdRow;

struct {
    struct {
        u32 rows, cols;
        struct { char *content; usize len; usize capacity; } buf;
    } scr;
    u16 cx, cy;
    u32 rx;
    EdRow *rows;
    u32 numrows;
    u32 rowoff;
    u32 coloff;
    struct termios prev_state;
} E = {0};


// screen
static inline void printscr(const char *s)
{
    for (u32 i = 0; s[i] != '\0'; i++) {
        da_append(E.scr.buf, s[i]);
    }
}

static inline void printnscr(const char *s, usize len)
{
    for (u32 i = 0; i < len; i++) {
        da_append(E.scr.buf, s[i]);
    }
}

static inline void writescr()
{
    usize n = write(STDOUT_FILENO, E.scr.buf.content, E.scr.buf.len);
    if (n != E.scr.buf.len) {
        perror("write");
        exit(1);
    }
    E.scr.buf.len = 0;
}

static void cursmov(u32 row, u32 col)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", row, col);
    printscr(buf);
}

Err curs_pos_fallback(u32 *rows, u32 *cols)
{
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return ERR_VOR;
    char buf[32];
    u32 i = 0;

    if (write(STDOUT_FILENO, GETCURS, LITLEN(GETCURS)) != 4) return ERR_VOR;

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[') return ERR_PARSING;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return ERR_PARSING;

    return ERR_OK;
}

static Err get_winsize(u32 *rows, u32 *cols)
{
    struct winsize wsiz;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &wsiz) == -1 || wsiz.ws_col == 0) {
        return curs_pos_fallback(rows, cols);
    }
    *cols = wsiz.ws_col;
    *rows = wsiz.ws_row;
    return ERR_OK;
}

static void restore_term()
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.prev_state);
    printscr(CLRSCR);
    cursmov(1, 1);
    writescr();
    if (E.scr.buf.content != NULL)
        free(E.scr.buf.content);
}

static void terminate(const char *s)
{
    printscr(CLRSCR);
    cursmov(1, 1);
    writescr();
    perror(s);
    exit(1);
}

static void enable_raw()
{
    if (tcgetattr(STDIN_FILENO, &E.prev_state) == -1) terminate("tcgetattr");
    atexit(restore_term);

    struct termios raw = E.prev_state;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_lflag &= ~(ISIG | IEXTEN);
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);

    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// editor
void editor_init()
{
    err(get_winsize(&E.scr.rows, &E.scr.cols));
    E.scr.rows -= 1;
}

u32 editor_row_cx_to_rx(EdRow *row, u32 cx)
{
    u32 rx = 0;
    for (u32 j = 0; j < cx; j++) {
        if (row->content[j] == '\t') {
            rx += (TABSTOP - 1) - (rx % TABSTOP);
        }
        rx++;
    }
    return rx;
}

void editor_update_row(EdRow *row)
{
    u32 tabs = 0;
    for (u32 j = 0; j < row->size; j++) {
        if (row->content[j] == '\t') tabs++;
    }
    free(row->render);
    row->render = malloc(sizeof(char) * (row->size + tabs * (TABSTOP - 1) + 1));
    assert(row != NULL);

    u32 j = 0;
    for (u32 i = 0; i < row->size; i++) {
        if (row->content[i] == '\t') {
            row->render[j++] = ' ';
            while (j % TABSTOP != 0) {
                row->render[j++] = ' ';
            }
        } else {
            row->render[j++] = row->content[i];
        }
    }
    row->render[j] = '\0';
    row->rsize = j;
}

void editor_append_row(char *s, isize len)
{
    E.rows = realloc(E.rows, sizeof(EdRow) * (E.numrows + 1));

    u32 at = E.numrows;
    E.rows[at].size = len;
    E.rows[at].content = malloc(len + 1);
    memcpy(E.rows[at].content, s, len);
    E.rows[at].content[len] = '\0';

    E.rows[at].rsize = 0;
    E.rows[at].render = NULL;
    editor_update_row(&E.rows[at]);

    ++E.numrows;

}

void editor_open(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (!fp) terminate("fopen");

    char *line = NULL;
    usize linecap = 0;
    isize linelen = 0;

    while ((linelen = getline(&line, &linecap, fp)) != -1) {
        while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r')) linelen--;
        editor_append_row(line, linelen);
    }
    free(line);
    fclose(fp);
}

i32 editor_listen_keypress(void)
{
    i32 n;
    char ch = 0;

    while ((n = read(STDIN_FILENO, &ch, 1)) != 1) {
        if (n == -1 && errno != EAGAIN) {
            terminate("read");
        }
    }

    if (ch == '\x1b') {
        char seq[3];

        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': case '7': return KEY_HOME;
                        case '4': case '8': return KEY_END;
                        case '5': return KEY_PAGE_UP;
                        case '6': return KEY_PAGE_DOWN;
                        case '3': return KEY_DEL;
                    }
                }
            } else {
                switch (seq[1]) {
                    case 'A': return KEY_UP;
                    case 'B': return KEY_DOWN;
                    case 'C': return KEY_RIGHT;
                    case 'D': return KEY_LEFT;
                    case 'H': return KEY_HOME;
                    case 'F': return KEY_END;
                }
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'H': return KEY_HOME;
                case 'F': return KEY_END;
            }
        }

        return '\x1b';
    }

    return ch;
}

void editor_mov_cursor(enum EditorKeys key)
{
    EdRow *row = (E.cy >= E.numrows) ? NULL : E.rows + E.cy;

    switch (key) {
        case KEY_UP:
            E.cy = clamp(E.cy - 1, 0, E.numrows);
            break;

        case KEY_RIGHT:
            if (row && E.cx < row->size) {
                E.cx++;
            } else if (E.cy < E.numrows) {
                E.cy++;
                E.cx = 0;
            }
            break;

        case KEY_DOWN:
            E.cy = clamp(E.cy + 1, 0, E.numrows);
            break;

        case KEY_LEFT:
            if (E.cx > 0) {
                E.cx--;
            } else if (E.cy > 0) {
                E.cy--;
                E.cx = E.rows[E.cy].size;
            }
            break;

        default: break;
    }

    row = (E.cy >= E.numrows) ? NULL : E.rows + E.cy;
    if (row) {
        E.cx = clamp(E.cx, 0, row->size);
    }
}

void editor_process_key()
{
    i32 ch = editor_listen_keypress();
    switch (ch) {
        case CTRL('q'):
            exit(EXIT_SUCCESS);
            break;

        case KEY_UP:
        case KEY_DOWN:
        case KEY_LEFT:
        case KEY_RIGHT:
            editor_mov_cursor(ch);
            break;

        case KEY_HOME:
            E.cx = 0;
            break;

        case KEY_END: {
                if (E.cy < E.numrows)
                    E.cx = E.rows[E.cy].size;
            } break;

        case KEY_DEL:
            break;

        case KEY_PAGE_DOWN: {
                E.cy = clamp(E.rowoff + E.scr.rows - 1, 0, E.numrows);
                u32 n = E.scr.rows;
                while (n--) editor_mov_cursor(KEY_DOWN);
            } break;

        case KEY_PAGE_UP: {
                E.cy = E.rowoff;
                u32 n = E.scr.rows;
                while (n--) editor_mov_cursor(KEY_UP);
            } break;

        default: {
                editor_mov_cursor(KEY_RIGHT);
            } break;
    }
}


void editor_draw_rows(void)
{
    for (u32 y = 0; y < E.scr.rows; y++) {
        u32 filerow = y + E.rowoff;

        if (filerow >= E.numrows) {
            if (E.numrows == 0 && y == E.scr.rows / 3) {
                const char *msg = "Welcome to clementine";
                u32 msglen = 0;
                while (msg[msglen] != '\0') msglen++;

                if (msglen > (u32)E.scr.cols) msglen = E.scr.cols;

                i32 leftpad = (E.scr.cols - (i32)msglen) / 2;
                if (leftpad > 0) {
                    printscr("~");
                    leftpad--;
                }
                while (leftpad-- > 0) printscr(" ");

                printscr(ANSI_RGB_FG(220, 120, 74));
                printnscr(msg, msglen);
                printscr(ANSI_RESET);
            } else {
                printscr("~");
            }
        } else {
            isize len = (isize)E.rows[filerow].rsize - (isize)E.coloff;
            printnscr(&E.rows[filerow].render[E.coloff], clamp(len, 0, E.scr.cols));
        }

        printscr("\r\n");
    }
}

void editor_scroll()
{
    E.rx = 0;
    if (E.cy < E.numrows) {
        E.rx = editor_row_cx_to_rx(&E.rows[E.cy], E.cx);
    }

    if (E.cy < E.rowoff) {
        E.rowoff = E.cy;
    }

    if (E.cy >= E.rowoff + E.scr.rows) {
        E.rowoff = E.cy - E.scr.rows + 1;
    }

    if (E.rx < E.coloff) {
        E.coloff = E.rx;
    }

    if (E.rx >= E.coloff + E.scr.cols) {
        E.coloff = E.rx - E.scr.cols + 1;
    }
}

static inline void editor_refresh()
{
    editor_scroll();

    printscr(CURSOFF);

    printscr(CLRSCR);
    cursmov(1, 1);

    editor_draw_rows();

    cursmov((E.cy - E.rowoff) + 1, (E.rx - E.coloff) + 1);

    printscr(CURSON);
    writescr();
}

i32 main(i32 argc, char **argv)
{
     enable_raw();
     editor_init();

     if (argc >= 2) {
         editor_open(argv[1]);
     }

     for (;;) {
         editor_refresh();
         editor_process_key();
     }
    return 0;
}
