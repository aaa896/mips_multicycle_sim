#include <stdlib.h>
#include "ctd.h"

typedef struct Chunk Chunk;
struct Chunk {
    Chunk *next;
    u8 *mem;
    u64 cap;
    u64 size;
};

#define ARENA_DEF_CAP 4096
typedef struct Arena Arena;
struct Arena {
    Chunk *head;
};

Chunk *create_chunk(u64 cap) {
    Chunk *chunk  = malloc(sizeof(Chunk));
    chunk->next = 0;
    chunk->mem = malloc(cap);
    chunk->cap = cap;
    chunk->size = 0;
    memset(chunk->mem, 0, chunk->cap);
    return chunk;
}

Arena * create_arena(u64 cap) {
    Arena *arena = malloc(sizeof(Arena));
    arena->head = create_chunk(cap);
    return arena;
}


#define ARENA_GRAN 16
void* insert_arena(Chunk *chunk, u64 bytes) {
    uintptr_t pos = (uintptr_t)chunk->mem + chunk->size;
    uintptr_t mod = pos % ARENA_GRAN;
    int bytes_forward = 0;
    if (mod) {
        pos += (ARENA_GRAN - mod);
        bytes_forward = ARENA_GRAN - mod;
        bytes += bytes_forward;
    }

    void *ret = 0;
    if (chunk->size + bytes > chunk->cap) {
        if (!chunk->next) chunk->next = create_chunk(chunk->cap * 2);
        ret = insert_arena(chunk->next, bytes - bytes_forward);
    } else {
        ret = (void*)pos;
        chunk->size += bytes;
    }
    return ret;
}

void free_chunks(Chunk *chunk) {
    if (!chunk) return;
    free_chunks(chunk->next);
    free(chunk->mem);
    free(chunk);
}

#define push_struct(chunk, item) \
    insert_arena((chunk), (sizeof(item)))
#define push_array(chunk, item, count) \
    insert_arena( (chunk), (sizeof(item) * (count)))
