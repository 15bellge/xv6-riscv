#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];

int nextktid = 1;
struct spinlock ktid_lock;

void forkret(void);

void kthreadinit(struct proc *p)
{
    initlock(&ktid_lock, "nexttid");

    for (struct kthread *kt = p->kthread; kt < &p->kthread[NKT]; kt++)
    {
        initlock(&kt->ktlock, "kthread");
        kt->ktstate = KT_UNUSED;

        kt->p = p;
        // WARNING: Don't change this line!
        // get the pointer to the kernel stack of the kthread
        kt->kstack = KSTACK((int)((p - proc) * NKT + (kt - p->kthread)));
    }
}

struct kthread *mykthread()
{
    push_off();
    struct kthread *kt = mycpu()->kt;
    pop_off();
    return kt;
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
        if (kt->ktstate == KT_UNUSED)
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
    kt->ktstate = KT_USED;

    // Allocate a trapframe page.
    if ((kt->trapframe = (struct trapframe *)kalloc()) == 0)
    {
        freekthread(kt);
        release(&kt->ktlock);
        return 0;
    }
    kt->trapframe = get_kthread_trapframe(p, kt);

    // Set up new context to start executing at forkret,
    // which returns to user space.
    memset(&kt->context, 0, sizeof(struct context));
    kt->context.ra = (uint64)forkret;
    kt->context.sp = kt->kstack + PGSIZE;
    return kt;
}

// free a thread structure and the data hanging from it,
// including user pages.
// kt->ktlock must be held.
void freekthread(struct kthread *kt)
{
    kt->trapframe = 0;
    kt->ktid = 0;
    kt->ktchan = 0;
    kt->ktkilled = 0;
    kt->ktxstate = 0;
    kt->ktstate = KT_UNUSED;
}

int kthread_create(void *(*start_func)(), void *stack, uint stack_size)
{
    struct proc *p = myproc();
    struct kthread *nkt;

    if ((nkt = allocthread()) == 0)
    {
        return -1;
    }

    acquire(&nkt->ktlock);
    nkt->ktstate = KT_RUNNABLE;
    release(&nkt->ktlock);

    nkt->trapframe->epc = start_func;     // user program counter
    nkt->trapframe->sp = stack;           // user stack pointer

    return nkt->ktid;
}

int kthread_kill(int ktid)
{
    struct proc *p;
    struct kthread *kt;

    for (p = proc; p < &proc[NPROC]; p++)
    {
        for (kt = p->kthread; kt < &p->kthread[NKT]; kt++)
        {
            acquire(&kt->ktlock);
            if (kt->ktid == ktid)
            {
                kt->ktkilled = 1;
                if (kt->ktstate == KT_SLEEPING)
                {
                    kt->ktstate = KT_RUNNABLE;
                }
            }
            release(&kt->ktlock);
            return 0;
        }
    }
    return -1;
}

void kthread_setkilled(struct kthread *kt)
{
    acquire(&kt->ktlock);
    kt->ktkilled = 1;
    release(&kt->ktlock);
}

int kthread_killed(struct kthread *kt)
{
    int k;

    acquire(&kt->ktlock);
    k = kt->ktkilled;
    release(&kt->ktlock);
    return k;
}

int kthread_exit(int status)
{
    struct kthread *kt = mykthread();
}

int kthread_join(int ktid, int status)
{
}
