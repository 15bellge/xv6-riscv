#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];

extern int nextktid = 1;
struct spinlock ktid_lock;

void forkret(void);
static void freekthread(struct kthread *kt);

void kthreadinit(struct proc *p)
{
    initlock(&ktid_lock, "nexttid");

    for (struct kthread *kt = p->kthread; kt < &p->kthread[NKT]; kt++)
    {
        // WARNING: Don't change this line!
        // get the pointer to the kernel stack of the kthread
        kt->kstack = KSTACK((int)((p - proc) * NKT + (kt - p->kthread)));
    }
}

struct kthread *mykthread()
{
    return &mycpu()->kt;
}

struct trapframe *get_kthread_trapframe(struct proc *p, struct kthread *kt)
{
    return p->base_trapframes + ((int)(kt - p->kthread));
}

int allocktid()
{
    int tid;

    acquire(&ktid_lock);
    tid = nextktid;
    nextktid = nextktid + 1;
    release(&ktid_lock);

    return tid;
}

struct kthread *allockthread(struct proc *p)
{
    struct kthread *kt;
    for (kt = p->kthread; kt < &p->kthread[NKT]; kt++)
    {
        acquire(&kt->ktlock);
        if (kt->ktstate == UNUSED)
        {
            goto found;
        }
        else
        {
            release(&kt->ktlock);
        }
    }
    return 0;

found:
    kt->ktid = allocktid();
    kt->ktstate = USED;

    // Allocate a trapframe page.
    // if ((kt->trapframe = (struct trapframe *)kalloc()) == 0)
    // {
    //     freekthread(kt);
    //     release(&kt->ktlock);
    //     return 0;
    // }
    kt->trapframe = get_kthread_trapframe(p, kt);

    // Set up new context to start executing at forkret,
    // which returns to user space.
    memset(&kt->context, 0, sizeof(kt->context));
    kt->context->ra = (uint64)forkret;
    kt->context->sp = kt->kstack + PGSIZE;

    return kt;
}

// free a thread structure and the data hanging from it,
// including user pages.
// kt->ktlock must be held.
void freekthread(struct kthread *kt)
{
    kt->kstack = 0;
    if (kt->trapframe)
        kfree((void *)kt->trapframe);
    kt->trapframe = 0;
    kt->ktid = 0;
    kt->ktchan = 0;
    kt->ktkilled = 0;
    kt->ktxstate = 0;
    kt->ktstate = UNUSED;
}

// void forkret(void)
// {
//     static int first = 1;

//     // Still holding p->lock from scheduler.
//     release(&myproc()->lock);

//     if (first)
//     {
//         // File system initialization must be run in the context of a
//         // regular process (e.g., because it calls sleep), and thus cannot
//         // be run from main().
//         first = 0;
//         fsinit(ROOTDEV);
//     }

//     usertrapret();
// }

// TODO: delte this after you are done with task 2.2
// void allocproc_help_function(struct proc *p)
// {
//     p->kthread->trapframe = get_kthread_trapframe(p, p->kthread);

//     p->context.sp = p->kthread->kstack + PGSIZE;
// }
