/**
 * @file: csim.c
 * @brief: Implementation of a cache simulator that simulates the behavior
 *         of a cache, given a series of memory operations
 *
 * This cache simulator implementation uses the least-recently used replacement
 * policy when choosing which cache line to evict, and follow a write-back,
 * write-allocate policy.
 *
 * @author: Ethan Lu <ethanl2@andrew.cmu.edu>
 */

#include "cachelab.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * @brief: A linked list element containing a tag and a dirty bit
 */
typedef struct cache_block {
    unsigned long tag;
    unsigned dirty;
    struct cache_block *next;
} cache_block_t;

/*
 * @brief: A queue structure representing a list of elements
 */
typedef struct cache_set {
    cache_block_t *head;
    cache_block_t *curr;
    int size;
    cache_block_t *tail;
} cache_set_t;

/**
 * @brief: Create a new cache block struct
 * @param[in] tag: The tag of the new cache block
 * @return: The new cache block, or NULL if memory allocation failed;
 */
cache_block_t *new_cache_block(unsigned long tag) {
    cache_block_t *new_block = malloc(sizeof(cache_block_t));

    if (!new_block) {
        return NULL;
    }

    new_block->tag = tag;
    new_block->dirty = 0;
    new_block->next = NULL;
    return new_block;
}

/**
 *  @brief: Create a new cache set struct
 *  @return: The new cache set, or NULL if memory allocation failed;
 */
cache_set_t *new_cache_set() {
    cache_set_t *new_set = malloc(sizeof(cache_set_t));

    if (!new_set) {
        return NULL;
    }

    new_set->head = NULL;
    new_set->curr = new_set->head;
    new_set->size = 0;
    new_set->tail = new_set->head;
    return new_set;
}

/**
 * @brief: Insert block at head of cache set
 * @param[in]   set: Pointer to cache set
 * @param[in] block: Pointer to cache block
 */
void cache_set_insert_head(cache_set_t *set, cache_block_t *block) {
    if (set) {
        block->next = set->head;
        set->head = block;
        set->curr = set->head;
        set->size++;
    }
}

/**
 * @brief: Attempts to remove block at current index of cache set
 * @param[in] set: Pointer to cache set
 * @return: A pointer to the cache block at the current index
 */
cache_block_t *cache_set_remove_current(cache_set_t *set) {
    if (set) {
        cache_block_t *saved;
        cache_block_t *pointer;
        if (set->head == set->curr) {
            saved = set->head;
            set->head = set->head->next;
            set->curr = set->head;
        } else if (set->tail == set->curr) {
            pointer = set->head;
            while (pointer->next != set->tail) {
                pointer = pointer->next;
            }
            saved = set->tail;
            set->tail = pointer;
            pointer->next = NULL;
            set->curr = set->tail;
        } else {
            pointer = set->head;
            while (pointer->next != set->curr) {
                pointer = pointer->next;
            }
            saved = set->curr;
            pointer->next = pointer->next->next;
        }
        set->size--;
        return saved;
    }
    return NULL;
}

/**
 * @brief: Attempts to remove block at tail of cache set
 * @param[in] set: Pointer to cache set
 */
void cache_set_remove_tail(cache_set_t *set) {
    if (set) {
        if (!set->head->next) {
            set->head = NULL;
        } else {
            cache_block_t *pointer = set->head;
            while (pointer->next->next != NULL) {
                pointer = pointer->next;
            }
            cache_block_t *removed = pointer->next;
            set->tail = pointer;
            set->tail->next = NULL;
            free(removed);
        }
    }
}

/**
 * @brief: Returns size of cache set
 * @param[in] set: Pointer to cache set
 * @return: Size of cache set
 */
int cache_set_size(cache_set_t *set) {
    if (set || set->head == set->tail) {
        return 0;
    }
    return set->size;
}

/**
 * @brief: Frees all memory used by a fully initialized cache
 * @param[in] cache: Pointer to an array of cache set
 * @param[in]     S: Number of sets in the cache
 */
void full_free_cache(cache_set_t **cache, unsigned long S) {
    if (cache) {
        for (unsigned long i = 0; i < S; i++) {
            cache_block_t *curr = cache[i]->head;
            cache_block_t *next;
            while (curr) {
                next = curr->next;
                free(curr);
                curr = next;
            }
        }

        for (unsigned long j = 0; j < S; j++) {
            free(cache[j]);
        }

        free(cache);
    }
}

/**
 * @brief: Frees all memory used by cache (without cache blocks)
 * @param[in] cache: Pointer to an array of cache set
 * @param[in]     i: Number of initialized sets in the cache
 */
void free_sets(cache_set_t **cache, unsigned long i) {
    if (cache) {
        for (unsigned long j = 0; j < i; j++) {
            free(cache[j]);
        }

        free(cache);
    }
}

/**
 * @brief: Change a cache block to be dirty
 * @param[in]    block: The cache block being looked at
 * @param[in] operator: Either 'S' or 'L'
 */
void set_dirty(cache_block_t *block, char operator) {
    if (operator== 'S') {
        block->dirty = 1;
    }
}

/**
 * @brief: Determine whether the cache set has a dirty tail
 * @param[in] set: The cache set being looked at
 */
unsigned dirty_tail(cache_set_t *set) {
    cache_block_t *pointer = set->head;
    while (pointer->next) {
        pointer = pointer->next;
    }
    return pointer->dirty;
}

/**
 * @brief: Tally the number of existing dirty cache blocks
 * @param[in] cache: Pointer to an array of cache set
 * @param[in]     S: The number of initialized cache sets
 */
int count_dirty(cache_set_t **cache, unsigned long S) {
    int dirty = 0;
    for (unsigned long i = 0; i < S; i++) {
        for (cache_block_t *curr = cache[i]->head; curr; curr = curr->next) {
            if (curr->dirty) {
                dirty++;
            }
        }
    }
    return dirty;
}

/**
 * @brief: Cache simulator
 * @param[in] argc: Number of arguments
 * @param[in] argv: Pointer to argument arrays
 */
int main(int argc, char *argv[]) {
    int v = 0, s = 0x7F800001, E = 0x7F800001, b = 0x7F800001;

    int t_initialized = 0;
    char *t;

    // @brief: Parse command-line arguments
    int opt;
    while ((opt = getopt(argc, argv, "vs:E:b:t:")) != -1) {
        switch (opt) {
        case 'v':
            v = 1;
            break;
        case 's':
            s = atoi(optarg);
            break;
        case 'E':
            E = atoi(optarg);
            break;
        case 'b':
            b = atoi(optarg);
            break;
        case 't':
            t = optarg;
            t_initialized = 1;
            break;
        default:
            printf("Incorrect Arguments");
            return 0;
        }
    }

    // @brief: Check if 's', 'E', 'b', and 't' have been set
    if (s == 0x7F800001) {
        printf("Missing <s> Argument");
        return 0;
    }

    if (E == 0x7F800001) {
        printf("Missing <E> Argument");
        return 0;
    }

    if (b == 0x7F800001) {
        printf("Missing <b> Argument");
        return 0;
    }

    if (!t_initialized) {
        printf("Missing <tracefile> Argument");
        return 0;
    }

    // @brief: build the cache
    unsigned long S = 1UL << s;

    cache_set_t **cache = (cache_set_t **)calloc(S, sizeof(cache_set_t *));

    unsigned long i = 0;
    for (; i < S; i++) {
        cache[i] = new_cache_set();
        if (!cache[i]) {
            free_sets(cache, i);
            return 0;
        }
    }

    // @brief: parse the lines of the file
    FILE *pFile;

    pFile = fopen(t, "r");

    if (!pFile) {
        full_free_cache(cache, S);
        printf("File Can Not Be Opened\n");
        return 0;
    }

    char operator;
    unsigned long address;
    int size;

    unsigned long hit = 0, miss = 0, eviction = 0;
    unsigned long dirty_byte = 0, dirty_eviction = 0;

    // @brief: start of the simulator
    while (0 < fscanf(pFile, "%c %lx, %d\n", &operator, &address, &size)) {
        const char *msg;

        unsigned long which_set = (address >> b) & (S - 1);
        unsigned long which_cache_line = (address >> (b + s));

        int found = 0, missed = 0, evicted = 0;

        // @brief: Search for an existing tag
        for (cache_block_t *pointer = cache[which_set]->head; pointer;
             pointer = pointer->next) {
            if (pointer->tag == which_cache_line) {
                while (cache[which_set]->curr != pointer) {
                    cache[which_set]->curr = cache[which_set]->curr->next;
                }
                cache_block_t *ru_block =
                    cache_set_remove_current(cache[which_set]);
                set_dirty(ru_block, operator);
                cache_set_insert_head(cache[which_set], ru_block);
                found = 1;
            }
        }

        if (!found) {
            // @brief: Insert a cache block with this new tag
            if (cache[which_set]->size < E) {
                cache_block_t *new_block = new_cache_block(which_cache_line);
                cache_set_insert_head(cache[which_set], new_block);
                set_dirty(new_block, operator);
                missed = 1;
            } else {
                // @brief: Evict the last cache block
                cache_block_t *new_block = new_cache_block(which_cache_line);
                if (dirty_tail(cache[which_set])) {
                    dirty_eviction++;
                }

                if (operator== 'L') {
                    new_block->dirty = 0;
                } else if (operator== 'S') {
                    new_block->dirty = 1;
                }
                cache_set_remove_tail(cache[which_set]);
                cache_set_insert_head(cache[which_set], new_block);
                evicted++;
            }
        }

        // @brief: Determine the outcome of the line
        int invalid_operator = 0;
        switch (operator) {
        case 'S':
            if (found) {
                hit++;
                msg = "hit";
            } else if (missed) {
                miss++;
                msg = "miss";
            } else if (evicted) {
                miss++;
                eviction++;
                msg = "miss eviction";
            }
            break;
        case 'L':
            if (found) {
                hit++;
                msg = "hit";
            } else if (missed) {
                miss++;
                msg = "miss";
                msg = "miss";
            } else if (evicted) {
                miss++;
                eviction++;
                msg = "miss eviction";
            }
            break;
        default:
            invalid_operator = 1;
        }

        if (invalid_operator) {
            full_free_cache(cache, S);
            printf("%c is not a valid operation\n", operator);
            return 0;
        }

        if (v) {
            printf("%c %lx, %d %s\n", operator, address, size, msg);
        }
    }

    dirty_byte = (unsigned long)(count_dirty(cache, S) * (1 << b));
    dirty_eviction = (unsigned long)(dirty_eviction * (1 << b));

    // @brief: Create summary
    csim_stats_t *stats = malloc(sizeof(csim_stats_t));

    stats->hits = hit;
    stats->misses = miss;
    stats->evictions = eviction;
    stats->dirty_bytes = dirty_byte;
    stats->dirty_evictions = dirty_eviction;

    fclose(pFile);
    full_free_cache(cache, S);
    printSummary(stats);
    free(stats);

    return 0;
}
