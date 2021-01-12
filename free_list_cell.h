#ifndef FREE_LIST_CELL
#define FREE_LIST_CELL

typedef struct free_list_cell {
    size_t size;
    struct free_list_cell* next;
} free_list_cell;

#endif
