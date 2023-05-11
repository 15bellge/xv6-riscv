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

// struct spinlock kthread_wait_lock;

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
    printf("in freekthread, kt: %p\n", kt);
    kt->trapframe = 0;
    kt->ktid = 0;
    kt->ktchan = 0;
    kt->ktkilled = 0;
    kt->ktxstate = 0;
    kt->ktstate = KT_UNUSED;
    // kt->p = 0;
}

int kthread_create(void *(*start_func)(), void *stack, uint stack_size)
{
    struct proc *p = myproc();
    struct kthread *nkt;

    if ((nkt = allockthread(p)) == 0)
    {
        return -1;
    }

    acquire(&nkt->ktlock);
    nkt->ktstate = KT_RUNNABLE;
    release(&nkt->ktlock);

    nkt->trapframe->epc = (uint64)start_func;          
    nkt->trapframe->sp = (uint64)(stack + stack_size - 1);

    return nkt->ktid;
}

int kthread_kill(int ktid)
{
    printf("in kthread_kill\n");
    struct proc *p;
    struct kthread *kt;

    for (p = proc; p < &proc[NPROC]; p++)
    {
        printf("in kthread_kill in for 1\n");
        for (kt = p->kthread; kt < &p->kthread[NKT]; kt++)
        {
            printf("in kthread_kill in for 2\n");
            acquire(&kt->ktlock);
            printf("in kthread_kill after acquire\n");
            if (kt->ktid == ktid)
            {
                printf("in kthread_kill in if 1\n");
                kt->ktkilled = 1;
                if (kt->ktstate == KT_SLEEPING)
                {
                    printf("in kthread_kill in if 2\n");
                    kt->ktstate = KT_RUNNABLE;
                }
            }
            release(&kt->ktlock);
            printf("in kthread_kill after release\n");
            return 0;
        }
        printf("in kthread_kill after for 2\n");
    }
    printf("in kthread_kill end\n");
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

void kthread_exit(int status)
{
    printf("in kthread_exit\n");
    struct kthread *mkt = mykthread();
    struct proc *p = mkt->p;
    
    acquire(&mkt->ktlock);
    mkt->ktstate = KT_ZOMBIE;
    mkt->ktxstate = status;
    release(&mkt->ktlock);

    int counter = 0;
    for (struct kthread *kt = p->kthread; kt < &p->kthread[NKT]; kt++)
    {
        acquire(&kt->ktlock);
        printf("kt->ktstate: %d\n", kt->ktstate);
        if (kt->ktstate == KT_ZOMBIE || kt->ktstate == KT_UNUSED)
        {
            printf("appending counter becuase kt->ktstate: %d\n", kt->ktstate);
            counter++;
        }
        release(&kt->ktlock);
    }
    printf("counter: %d\n", counter);
    if (counter == NKT)
    {        
        exit(status);
    }
    acquire(&mkt->ktlock);
    sched();
    release(&mkt->ktlock);
}

int kthread_join(int ktid, int status)
{
    printf("in kthread_join\n");
    struct kthread *mkt = mykthread();
    struct proc *p = mkt->p;
    acquire(&myproc()->lock);

    for (;;)
    {
        for (struct kthread *kt = p->kthread; kt < &p->kthread[NKT]; kt++)
        {
            acquire(&kt->ktlock);
            if (kt->ktid == ktid && kt->ktstate == KT_ZOMBIE)
            {
                if (status != 0 && copyout(p->pagetable, status, (char *)&kt->ktxstate,
                                           sizeof(kt->ktxstate)) < 0)
                {
                    release(&kt->ktlock);
                    release(&myproc()->lock);
                    return -1;
                }
                if(kt != mykthread())
                    freekthread(kt);
                release(&kt->ktlock);
                release(&myproc()->lock);
                return ktid;
            }
            release(&kt->ktlock);
            if(kthread_killed(kt)){
                release(&myproc()->lock);
                return -1;
            }
        }
        printf("in kthread_join before sleep\n");
        sleep(mkt, &myproc()->lock);
        return 0;
    }
}
