#include "ustack.h"

// typedef long Align;

// union header
// {
//     struct
//     {
//         union header *ptr;
//         uint size;
//     } s;
//     Align x;
// };

// typedef union header Header;

// static Header base;
// static Header *freep;
static char *stack_ptr;

// static Header *
// morecore(uint nu)
// {
//     char *p;
//     Header *hp;

//     if (nu < 4096)
//         nu = 4096;
//     p = sbrk(nu * sizeof(Header));
//     if (p == (char *)-1)
//         return 0;
//     hp = (Header *)p;
//     hp->s.size = nu;
//     free((void *)(hp + 1));
//     return freep;
// }

void *ustack_malloc(uint len)
{
    // Align the size to a multiple of sizeof(uint)
    uint size = (len + sizeof(uint) - 1) & ~(sizeof(uint) - 1);
    if (len > MAX_ALLOC)
    {
        return -1;
    }
    // If stack_ptr is not set or the remaining space is not enough, request a new page
    if (stack_ptr == 0 || (stack_ptr - (char *)sbrk(0)) < (int)(size + sizeof(uint)))
    {
        char *page = sbrk(PGSIZE);
        if (page == (char *)-1)
        {
            return -1; // Failed to allocate memory
        }
        stack_ptr = page;
    }
    // Store the current stack pointer in the allocated memory block
    char *ptr = stack_ptr;
    *((uint *)ptr) = (uint)(stack_ptr - (char *)sbrk(0));
    stack_ptr += size + sizeof(uint);

    // Return a pointer to the allocated memory block (excluding the size field)
    return (void *)(ptr + sizeof(uint));
}

int ustack_free(void *ptr)
{
    if (ptr == 0)
    {
        return -1;
    }

    // Get the size of the allocated memory block from the size field
    char *block = (char *)ptr - sizeof(uint);
    uint size = *((uint *)block);

    // Roll back the stack pointer to the previous block
    stack_ptr = block;

    // If the previous block crosses a page boundary, free the page using a negative sbrk() argument
    if ((stack_ptr - (char *)sbrk(0)) <= 0 && (stack_ptr - (char *)sbrk(0)) % PGSIZE != 0)
    {
        sbrk(-(PGSIZE - ((stack_ptr - (char *)sbrk(0)) % PGSIZE)));
    }
}