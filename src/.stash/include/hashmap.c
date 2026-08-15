#include <assert.h>

#include "itypes.h"
#include "da.h"

#define NOT_FOUND U32_MAX

typedef enum {
    Empty = -32,
    EOC = -64, // End Of Chain
} CellState;

typedef struct {
    u32 val;
    u32 key;
    union { isize link; CellState state; };
} HashCell;

typedef struct {
    HashCell* content;
    usize len;
    usize capacity;
} HashResrv;

typedef struct {
    HashCell *store;
    usize size;
    HashResrv resrv;
} HashMap;

u32 hash(u32 key)
{
    key = ((key >> 16) ^ key) * 0x45d9f3bu;
    key = ((key >> 16) ^ key) * 0x45d9f3bu;
    key = (key >> 16) ^ key;
    return key;
}

HashMap map_init(usize size)
{
    HashMap map;
    map.size = size;
    map.store = malloc(sizeof(HashCell) * size);
    assert(map.store != NULL);
    for (usize i = 0; i < size; i++) {
        map.store[i] = (HashCell) {
            .val = 0,
            .key = 0,
            .state = Empty
        };
    }
    map.resrv = (HashResrv){0};
    return map;
}

void map_free(HashMap *map)
{
    free(map->store);
    free(map->resrv.content);
}

void map_set(HashMap *map, u32 key, u32 val)
{
    HashCell new_cell = { .key = key, .val = val, .state = EOC };
    u32 i = hash(key) % map->size;
    HashCell *cell = &map->store[i];

    if (cell->state == Empty) {
        map->store[i] = new_cell;
    }
    else if (cell->state == EOC) { // collision but single no links.
        if (cell->key == key)
            cell->val = val;
        else {
            da_append(map->resrv, new_cell);
            cell->link = map->resrv.len - 1;
        }
    }
    else { // there is a chain
        cell = &map->resrv.content[cell->link];
        for (; cell->state >= 0; cell = &map->resrv.content[cell->link]) {
            if (cell->key == key) {
                cell->val = val;
                break;
            }
            else {
                da_append(map->resrv, new_cell);
                cell->link = map->resrv.len - 1;
                break;
            }
        }
    }

    
}

u32 map_get(HashMap* map, u32 key)
{
    u32 i = hash(key) % map->size;
    HashCell cell = map->store[i];
    if (cell.state == Empty) {
        return NOT_FOUND;
    }
    if (cell.key != key) { // find collided value
        if (cell.state == EOC)
            return NOT_FOUND;

        cell = map->resrv.content[cell.link];
        while (cell.state != EOC && cell.key != key)
            cell = map->resrv.content[cell.link];

        if (cell.key == key)
            return cell.val;
        else
            return NOT_FOUND;
    }
    return cell.val;
}

// Must free *dest after use
void hash_get_keys(HashMap* map, u32 **dest, u32 *len)
{
    struct { u32 *content; usize len; usize capacity; } keys = {0};

    for (u32 i = 0; i < map->size; i++) {
        HashCell cell = map->store[i];
        if (cell.state == Empty)
            continue;

        da_append(keys, cell.key);
        if (cell.state != EOC) {
            isize curr = cell.link;
            while (curr != EOC) {
                HashCell cell = map->resrv.content[curr];
                da_append(keys, cell.key); 
                curr = cell.link;
            }
        }
    }

    *dest = keys.content;
    *len = keys.len;
}

i32 main()
{
    HashMap map = map_init(256);
    map_set(&map, 23, 69);
    map_set(&map, 25, 420);
    map_set(&map, 25, 720);
    map_set(&map, 23, 70);

    u32 *keys = NULL, len = 0;
    hash_get_keys(&map, &keys, &len);
    for (u32 i = 0; i < len; i++) {
        printf("%u ", keys[i]);
    }
    free(keys);
    map_free(&map);
    return 0;
}
