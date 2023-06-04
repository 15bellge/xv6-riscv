#include "ustack.h"

struct Header
{
    uint len;
    uint dealloc_page;
    struct Header *prev;
};

static struct Header *header_ptr = 0;
static struct Header *base = 0;
static char *stack_ptr;
static char *page_ptr;

void *ustack_malloc(uint len)
{
    struct Header *hp = 0;
    char *page;

    if (len > MAX_ALLOC)
    {
        return (void *)-1;
    }

    if (header_ptr == base)
    {
        page = sbrk(PGSIZE);
        if (page == (char *)-1)
        {
            return (void *)-1;
        }
        page_ptr = page + PGSIZE;
        hp = (struct Header *)page;
        hp->len = len;
        hp->prev = base;
        stack_ptr = (char *)hp + len;
        header_ptr = hp;
        return (void *)stack_ptr;
    }

    if (page_ptr - stack_ptr < len)
    {
        page = sbrk(PGSIZE);
        if (page == (char *)-1)
        {
            return (void *)-1;
        }
        page_ptr = page + PGSIZE;
        hp->len = len;
        hp->prev = header_ptr;
        hp->dealloc_page = 1;
        stack_ptr = (char *)hp + len;
        header_ptr = hp;
        return (void *)stack_ptr;
    }

    else
    {
        hp = (struct Header *)stack_ptr;
        hp->len = len;
        hp->prev = header_ptr;
        stack_ptr = (char *)hp + len;
        header_ptr = hp;
        return (void *)stack_ptr;
    }
}

int ustack_free(void)
{
    char *page;
    int header_size = header_ptr->len;
    header_ptr = header_ptr->prev;
    if (header_ptr->dealloc_page == 1)
    {
        page = sbrk(-PGSIZE);
        if (page == (char *)-1)
        {
            return -1;
        }
        page_ptr -= PGSIZE;
    }
    return header_size;
}