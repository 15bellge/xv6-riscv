#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
    int n;
    argint(0, &n);
    char msg[32];                // task3
    argstr(1, msg, strlen(msg)); // task3
    exit(n, msg);
    return 0; // not reached
}

uint64
sys_getpid(void)
{
    return myproc()->pid;
}

uint64
sys_fork(void)
{
    return fork();
}

uint64
sys_wait(void)
{
    uint64 p;
    argaddr(0, &p);
    char msg[32];                // task3
    argstr(1, msg, strlen(msg)); // task3
    return wait(p, msg);
}

uint64
sys_sbrk(void)
{
    uint64 addr;
    int n;

    argint(0, &n);
    addr = myproc()->sz;
    if (growproc(n) < 0)
        return -1;
    return addr;
}

uint64
sys_sleep(void)
{
    int n;
    uint ticks0;

    argint(0, &n);
    acquire(&tickslock);
    ticks0 = ticks;
    while (ticks - ticks0 < n)
    {
        if (killed(myproc()))
        {
            release(&tickslock);
            return -1;
        }
        sleep(&ticks, &tickslock);
    }
    release(&tickslock);
    return 0;
}

uint64
sys_kill(void)
{
    int pid;

    argint(0, &pid);
    return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
    uint xticks;

    acquire(&tickslock);
    xticks = ticks;
    release(&tickslock);
    return xticks;
}

// task2
uint64
sys_memsize(void)
{
    struct proc *p = myproc();
    if (p != 0)
    {
        return myproc()->sz;
    }
    return -1;
}

// task5
uint64
sys_setpspriority(void)
{
    int priority;
    argint(0, &priority);
    set_ps_priority(priority);
    return 0;
}

//task6
uint64
sys_set_cfs_priority(void)
{
    int priority;
    argint(0, &priority);
    return set_cfs_priority(priority);
    return 0; //not reached
}

uint64
sys_get_cfs_stats(void){
    int pid;
    argint(0, &pid);
    get_cfs_stats(pid);
    return 0;
}

// task7
uint64
sys_setpolicy(void)
{
    int policy;
    argint(0, &policy);
    set_policy(policy);
    return 0;
}
