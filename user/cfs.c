#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/cfs_stats.h"
/*
This doesn't work!
*/
void loop()
{
    for (int i = 0; i < 1000000; i++)
    {
        if (i % 100000 == 0)
        {
            sleep(1);
        }
    }
}

void print_stats(int pid)
{
    struct cfs_stats *stats = get_cfs_stats(pid);
    //printf("PID: %d\nCFS priority: %d\nrun time: %d\nsleep time: %d\nrunnable time: %d\n", stats->cfs_priority, stats->rtime, stats->stime, stats->retime);
    printf("PID: %d,\tCFS priority: %d,\trun time: %d,\tsleep time: %d,\trunnable time: %d\n", stats->cfs_priority, stats->rtime, stats->stime, stats->retime);
}

void main(int argc, char *argv[])
{
    int low, normal, high;

    low = fork();
    if (low == 0) // in low
    {
        set_cfs_priority(0);
        sleep(10);
        loop();
        print_stats(low);
        exit(0, "");
    }
    else
    {
        normal = fork();
        if (normal == 0) // in normal
        {
            set_cfs_priority(1);
            sleep(20);
            loop();
            print_stats(normal);
            exit(0, "");
        }
        else
        {
            high = fork();
            if (high == 2) // in high
            {
                set_cfs_priority(1);
                sleep(30);
                loop();
                print_stats(high);
                exit(0, "");
            }
            else
            {
                wait(0, "");
                exit(0, "");
            }
        }
    }
    exit(0, "");
}