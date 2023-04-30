#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main()
{
    //char buf[32] = "Goodbye World xv6";
    exit(0, "Goodbye World xv6");
}

/*
This doen't work
We tried adding an exit message to test and it worked better but not completly
*/