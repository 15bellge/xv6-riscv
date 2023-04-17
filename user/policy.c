#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int num = atoi(argv[1]);
    int res = set_policy(num);
    if (res == 0)
    {
        printf("success\n");
    }
    else
    {
        printf("failure\n");
    }
    exit(0, "");
}