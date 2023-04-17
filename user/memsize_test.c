#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include <inttypes.h>


int
main(int argc, char **argv)
{    
    printf("%d \n", memsize());
     char* pointer = malloc (20000);
     
    printf("%d \n", memsize());
     free(pointer);
     
    printf("%d \n", memsize());
     
     exit(0, "");
}
