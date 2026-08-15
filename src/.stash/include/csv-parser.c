#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "itypes.h"
#include "utils.h"

#define ARENA_IMPLEMENTATION
#include "arena.h"

#define DA_INIT_SIZE 64
#include "da.h"

#define TOTAL_TYPES 5
typedef enum {
    CSVText    = 0,
    CSVNumeric = 1,
    CSVBool    = 2,
    CSVTempo   = 3,
    CSVEmpty   = 4,
} CSVCellType;

typedef struct {
    CSVCellType type;
    u32 col_no;
} CSVMeta;

typedef struct {
    char* content;
    usize len;
    usize capacity;
} String;

typedef struct {
    CSVMeta meta;
    String data;
} CSVCell;

typedef struct {
    CSVCell *content;
    u32 len;
    u32 capacity;
} CSVRow;

typedef struct {
    CSVRow **content;
    usize len;
    usize capacity;
} CSVTable;

typedef struct {
    CSVCellType *content;
    u32 len;
    u32 capacity;
} Schema;

typedef struct {
    const char* raw;
    u32 raw_len;
    Schema schema;
    bool has_header;
    CSVTable table;
} CSV;

typedef enum {
    CSVQuotedState, // handling single or double quotes
    CSVNormState, // for handling text
} CSVState;

static inline void debug_type(CSVCellType type)
{
    switch (type) {
        case CSVBool: printf("Bool "); break;
        case CSVEmpty: printf("Empty "); break;
        case CSVNumeric: printf("Numeric "); break;
        case CSVTempo: printf("Tempo "); break;
        case CSVText: printf("Text "); break;
    }
}

static CSVCellType determine_cell_type(String *data)
{
    u8 DEBUG = 0;
    debug("[ ");
    for (u32 i = 0; i < data->len; i++) debug("%c", data->content[i]);
    debug(": ");
    if (data->len == 0) {
        debug("Empty ]\n");
        return CSVEmpty;
    }
    if (data->len < 6) {
        u8 is_bool = false;
        if (tolower(*data->content) == 't') {
            const char* phrase = "true"; is_bool = true;
            for (u8 i = 1; i < data->len; i++) {
                is_bool &= phrase[i] == tolower(data->content[i]);
            }
        } else if (tolower(*data->content) == 'f') {
            const char* phrase = "false"; is_bool = true;
            for (u8 i = 1; i < data->len; i++) {
                is_bool &= phrase[i] == tolower(data->content[i]);
            }
        }
        if (is_bool) {
            debug("Bool ]\n");
            return CSVBool;        
        }
    }

    enum {
        Tempo   = 1,
        Numeric = 2,
        Text    = 4,        
        Sign    = 8,
        Frac    = 16,
    };
    u8 opts = 0;
    for (u32 i = 0; i < data->len; i++) {
        char ch = data->content[i];
        if (i == 0 && (ch == '+' || ch == '-')) {
            opts |= Sign;
        } else if (i == 4 && opts & Numeric && (ch == '-' || ch == '/')) { // yyyy-
            if (opts & (Sign | Frac) || data->len > strlen("yyyy-mm-dd")) {
                goto RET_CSV_TEXT;
            }
            opts = Tempo;
        } else if (i == 7) {
            if (opts & Tempo && ch != '-' && ch != '/') {
                goto RET_CSV_TEXT;
            }
        } else if (ch >= '0' && ch <= '9') {
            if (opts & Tempo) { continue; }
            opts |= Numeric;
        } else if (ch == '.') {
            if (opts & (Tempo | Frac)) {
                goto RET_CSV_TEXT;
            }
            opts |= Frac;
        } else {
            goto RET_CSV_TEXT;
        }
        
    }
    if (opts & Tempo) {
        debug("Tempo ]\n");
        return CSVTempo;
    } else if (opts & Numeric) {
        debug("Numeric ]\n");
        return CSVNumeric;
    }

    RET_CSV_TEXT:
        debug("Text ]\n");
        return CSVText;
}

CSVCell csv_cell(String *data, u32 col_no)
{
    CSVCell cell = {0};
    cell.data = *data;
    cell.meta = (CSVMeta) {
        .type   = determine_cell_type(data),
        .col_no = col_no
    };
    return cell;
}

void csv_commit_row(CSVTable *table, const CSVRow* row)
{
    CSVRow *new_row = malloc(sizeof(CSVRow));
    assert(new_row != NULL);
    *new_row = *row;
    da_append(*table, new_row);
}

CSVCellType get_most_frequent_type(u16 *chart)
{
    u16 most_frequent = CSVText, top_score = chart[0];
    u16 second_most_frequent = CSVText;
    for (u16 i = 1; i < TOTAL_TYPES; i++) {
        if (chart[i] > top_score) {
            second_most_frequent = most_frequent;
            most_frequent = i;
            top_score = chart[most_frequent];
        }
    }

    if (most_frequent == CSVEmpty)
        return second_most_frequent;

    return most_frequent;
}

static void determine_schema(CSV* csv)
{
    Arena *arena = arena_init(1024);

    const u16 MAX_SAMPLES = 512;
    CSVTable *table = &csv->table;

    u32 cols = table->content[0][0].len;
    u16 **ranks = arena_alloc(arena, cols * sizeof(u16*));
    for (u32 i = 0; i < cols; i++) {
        ranks[i] = arena_alloc(arena, sizeof(u16) * TOTAL_TYPES);
        memset(ranks[i], 0, sizeof(u16) * TOTAL_TYPES);
    }

    u32 rows = table->len;
    for (u32 i = csv->has_header ? 1 : 0; i < rows && i < MAX_SAMPLES; i++) {
        CSVRow *row = table->content[i];
        for (u32 j = 0; j < cols; j++) {
            CSVCellType type = row->content[j].meta.type;
            ranks[j][type] += 1;
        }
    }

    for (u32 i = 0; i < cols; i++) {
        CSVCellType type = get_most_frequent_type(ranks[i]);
        da_append(csv->schema, type);
    }
    arena_free(arena);
}

CSV csv_parse(const char* raw)
{
    u8 DEBUG = 0;
    CSV csv = (CSV) {
        .raw        = raw,
        .raw_len    = strlen(raw),
        .schema     = {0},
        .table      = {0},
        .has_header = false,
    };

    CSVState state = CSVNormState;
    String token = {0};
    CSVRow row = {0};
    u32 col_no = 0;
    char quote_key;

    for (u32 i = 0; i < csv.raw_len; i++) {
        char ch = csv.raw[i];
        switch (state) {
            case CSVNormState:
                if (token.len == 0 && (ch == ' ' || ch == '\t')) {
                    continue;
                }
                else if (ch == ',') {
                    debug(" [%d] ", col_no);
                    da_append(row, csv_cell(&token, col_no));
                    token = (String){0};
                    col_no++;
                }
                else if (ch == '\n') {
                    debug(" [eol]\n");
                    da_append(row, csv_cell(&token, col_no));
                    csv_commit_row(&csv.table, &row);
                    token = (String){0};
                    row = (CSVRow){0};
                    col_no = 0;
                }
                else if (ch == '\'' || ch == '"') {
                    debug(" [Q] ");
                    quote_key = ch;
                    state = CSVQuotedState;
                }
                else {
                    debug("%c", ch);
                    da_append(token, ch);
                }
                break;
            case CSVQuotedState:
                if (ch == quote_key) {
                    if (i + 1 < csv.raw_len && csv.raw[i + 1] == quote_key) {
                        debug("%c", quote_key);
                        da_append(token, quote_key);
                        i++;
                    } else {
                        debug(" [N] ");
                        state = CSVNormState;
                    }
                } else {
                    debug("%c", ch);
                    da_append(token, ch);
                }
                break;
        }
    }
    if (row.len != 0) {
        debug(" [eol]\n");
        da_append(row, csv_cell(&token, col_no));
        csv_commit_row(&csv.table, &row);
    }

    if (csv.table.len > 0) {
        CSVRow *header = csv.table.content[0];
        CSVCellType type = header->content[0].meta.type;
        csv.has_header = true;
        for (u32 i = 0; i < header->len; i++) {
            if (header->content[i].meta.type != type) {
                csv.has_header = false;
                break;
            }
        }
    }

    determine_schema(&csv);
    return csv;
}

void csv_free(CSV *csv)
{
    if (csv == NULL || csv->table.content == NULL) return;
    CSVTable *table = &csv->table;
    
    for (u32 i = 0; i < table->len; i++) { 
        CSVRow *row = table->content[i];
        if (row == NULL) continue;

        for (u32 j = 0; j < row->len; j++) { 
            CSVCell *cell = &row->content[j];
            if (cell->data.content != NULL) {
                free(cell->data.content);
            }
        }
        
        free(row->content);
        free(row);
    }
    
    free(table->content);
    free(csv->schema.content);
    *csv = (CSV){0};
}

const char* test_input = 
    "ID,Title,Author,Published,Flags\n"
    "1,\"\"\"Data Structures\"\"\", Hari, 2026-02-26, False\n"
    "2,\"\"\"idk\"\"\", hariii, 2026-02-28, True\n"
    "3,\"\"\"Introduction to Algorithms & Analysis\"\"\", Cormen, 2021-07-15, TRUE\n"
    "4,\"\"\"The C Programming Language (2nd Ed.)\"\"\", Brian W. Kernighan & Dennis M. Ritchie, 1988-04-01, false\n"
    "5,\"\"\"\"\"\", Unknown Author, 2000-01-01, False\n"
    "6,\"\"\"A Byte of Python\"\"\", Swaroop C.H., , true\n"
    "7,\"\"\"Design Patterns: Elements of Reusable Object-Oriented Software\"\"\", Erich Gamma; Richard Helm; Ralph Johnson; John Vlissides, 1994-10-31, False\n"
    "8,\"\"\"X\"\"\", A, 2026-06-25, True\n"
    "9,\"\"\"Automate the Boring Stuff with Python: Practical Programming for Total Beginners\"\"\", Al Sweigart, 2019-11-12, false\n"
    "10,\"\"\"Compilers: Principles, Techniques, and Tools\"\"\", Alfred V. Aho; Monica S. Lam; Ravi Sethi; Jeffrey D. Ullman, 2006-09-04, True\n";

i32 main()
{
    CSV csv = csv_parse(test_input);
    csv_free(&csv);

    return 0;
}
