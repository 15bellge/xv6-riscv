#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

extern struct cpu cpus[NCPU];

extern struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;
extern int nextktid;

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void proc_mapstacks(pagetable_t kpgtbl)
{
    struct proc *p;

    for (p = proc; p < &proc[NPROC]; p++)
    {
        for (struct kthread *kt = p->kthread; kt < &p->kthread[NKT]; kt++)
        {
            char *pa = kalloc();
            if (pa == 0)
                panic("kalloc");
            uint64 va = KSTACK((int)((p - proc) * NKT + (kt - p->kthread)));
            kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
        }
    }
}

// initialize the proc table.
void procinit(void)
{
    struct proc *p;

    initlock(&pid_lock, "nextpid");
    initlock(&wait_lock, "wait_lock");
    for (p = proc; p < &proc[NPROC]; p++)
    {
        initlock(&p->lock, "proc");
        p->state = UNUSED;
        kthreadinit(p);
    }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int cpuid()
{
    int id = r_tp();
    return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu *
mycpu(void)
{
    int id = cpuid();
    struct cpu *c = &cpus[id];
    return c;
}

// Return the current struct proc *, or zero if none.
struct proc *
myproc(void)
{
    printf("in myproc\n");
    // push_off();
    // //struct cpu *c = mycpu();
    // //printf("c: %p\n", c);
    // struct kthread *kt = mykthread();
    // printf("kt: %p\n", kt);
    // struct proc *p = kt->p;
    // printf("p: %p\n", p);
    // pop_off();
    // return p;
    return mykthread()->p;
}

int allocpid()
{
    printf("in allocpid\n");
    int pid;

    acquire(&pid_lock);
    pid = nextpid;
    nextpid = nextpid + 1;
    release(&pid_lock);

    return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc *
allocproc(void)
{
    printf("in allocproc\n");
    struct proc *p;

    for (p = proc; p < &proc[NPROC]; p++)
    {
        acquire(&p->lock);
        if (p->state == UNUSED)
        {
            goto found;
        }
        else
        {
            release(&p->lock);
        }
    }
    return 0;

found:
    printf("in allocproc after allocthread\n");
    p->pid = allocpid();
    p->state = USED;

    // Allocate a trapframe page.
    if ((p->base_trapframes = (struct trapframe *)kalloc()) == 0)
    {
        freeproc(p);
        release(&p->lock);
        return 0;
    }

    // An empty user page table.
    p->pagetable = proc_pagetable(p);
    if (p->pagetable == 0)
    {
        freeproc(p);
        release(&p->lock);
        return 0;
    }

    // Set up new context to start executing at forkret,
    // which returns to user space.
    // memset(&p->context, 0, sizeof(p->context));
    // p->context.ra = (uint64)forkret;
    // p->context.sp = p->kstack + PGSIZE;

    // // TODO: delte this after you are done with task 2.2
    // allocproc_help_function(p);
    printf("in allocproc before return\n");
    // task2
    nextktid = 1;
    allockthread(p);

    return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
    printf("in freeproc\n");
    for (struct kthread *kt = p->kthread; kt < &p->kthread[NKT]; kt++)
    {
        freekthread(kt);
    }
    if (p->base_trapframes)
        kfree((void *)p->base_trapframes);
    p->base_trapframes = 0;
    if (p->pagetable)
        proc_freepagetable(p->pagetable, p->sz);
    p->pagetable = 0;
    p->sz = 0;
    p->pid = 0;
    p->parent = 0;
    p->name[0] = 0;
    p->chan = 0;
    p->killed = 0;
    p->xstate = 0;
    p->state = UNUSED;
}

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
pagetable_t
proc_pagetable(struct proc *p)
{
    printf("in proc_pagetable\n");
    pagetable_t pagetable;

    // An empty page table.
    pagetable = uvmcreate();
    if (pagetable == 0)
        return 0;

    // map the trampoline code (for system call return)
    // at the highest user virtual address.
    // only the supervisor uses it, on the way
    // to/from user space, so not PTE_U.
    if (mappages(pagetable, TRAMPOLINE, PGSIZE,
                 (uint64)trampoline, PTE_R | PTE_X) < 0)
    {
        uvmfree(pagetable, 0);
        return 0;
    }

    // map the trapframe page just below the trampoline page, for
    // trampoline.S.
    if (mappages(pagetable, TRAPFRAME(0), PGSIZE,
                 (uint64)(p->base_trapframes), PTE_R | PTE_W) < 0)
    {
        uvmunmap(pagetable, TRAMPOLINE, 1, 0);
        uvmfree(pagetable, 0);
        return 0;
    }

    return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
    printf("in proc_freepagetable\n");
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmunmap(pagetable, TRAPFRAME(0), 1, 0);
    uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// assembled from ../user/initcode.S
// od -t xC ../user/initcode
uchar initcode[] = {
    0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
    0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
    0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
    0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
    0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
    0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00};

// Set up first user process.
void userinit(void)
{
    printf("in userinit\n");
    struct proc *p;

    p = allocproc();
    printf("in userinit after allocproc\n");
    initproc = p;
    // allocate one user page and copy initcode's instructions
    // and data into it.
    uvmfirst(p->pagetable, initcode, sizeof(initcode));
    p->sz = PGSIZE;
    p->kthread[0].trapframe->epc = 0;     // user program counter
    p->kthread[0].trapframe->sp = PGSIZE; // user stack pointer
    safestrcpy(p->name, "initcode", sizeof(p->name));
    p->cwd = namei("/");

    // p->state = RUNNABLE;
    printf("in userinit before task 2\n");
    // task2
    p->kthread[0].ktstate = KT_RUNNABLE;
    release(&p->kthread[0].ktlock);
    release(&p->lock);
    printf("in userinit end\n");
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int growproc(int n)
{
    printf("in growproc\n");
    uint64 sz;
    struct proc *p = myproc();

    sz = p->sz;
    if (n > 0)
    {
        if ((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0)
        {
            return -1;
        }
    }
    else if (n < 0)
    {
        sz = uvmdealloc(p->pagetable, sz, sz + n);
    }
    p->sz = sz;
    return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int fork(void)
{
    printf("in fork\n");
    int i, pid;
    struct proc *np;
    struct proc *p = myproc();
    struct kthread *kt = mykthread();

    // Allocate process.
    if ((np = allocproc()) == 0)
    {
        return -1;
    }

    // Copy user memory from parent to child.
    if (uvmcopy(p->pagetable, np->pagetable, p->sz) < 0)
    {
        freeproc(np);
        release(&np->lock);
        return -1;
    }
    np->sz = p->sz;

    // copy saved user registers.
    *(np->kthread[0].trapframe) = *(kt->trapframe);

    // Cause fork to return 0 in the child.
    np->kthread[0].trapframe->a0 = 0;

    // increment reference counts on open file descriptors.
    for (i = 0; i < NOFILE; i++)
        if (p->ofile[i])
            np->ofile[i] = filedup(p->ofile[i]);
    np->cwd = idup(p->cwd);

    safestrcpy(np->name, p->name, sizeof(p->name));

    pid = np->pid;

    release(&np->lock);

    acquire(&wait_lock);
    np->parent = p;
    release(&wait_lock);

    // task2
    acquire(&np->kthread[0].ktlock);
    np->kthread[0].ktstate = KT_RUNNABLE;
    release(&np->lock);

    return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void reparent(struct proc *p)
{
    printf("in reparent\n");
    struct proc *pp;

    for (pp = proc; pp < &proc[NPROC]; pp++)
    {
        if (pp->parent == p)
        {
            pp->parent = initproc;
            wakeup(initproc);
        }
    }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void exit(int status)
{
    printf("in exit\n");
    struct proc *p = myproc();

    if (p == initproc)
        panic("init exiting");

    // Close all open files.
    for (int fd = 0; fd < NOFILE; fd++)
    {
        if (p->ofile[fd])
        {
            struct file *f = p->ofile[fd];
            fileclose(f);
            p->ofile[fd] = 0;
        }
    }

    begin_op();
    iput(p->cwd);
    end_op();
    p->cwd = 0;

    acquire(&wait_lock);

    // Give any children to init.
    reparent(p);

    // Parent might be sleeping in wait().
    wakeup(p->parent);

    acquire(&p->lock);

    p->xstate = status;
    p->state = ZOMBIE;

    // task2
    acquire(&p->kthread->ktlock);
    p->kthread->ktstate = KT_ZOMBIE;
    release(&p->kthread->ktlock);

    release(&wait_lock);

    // Jump into the scheduler, never to return.
    sched();
    panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int wait(uint64 addr)
{
    printf("in wait\n");
    struct proc *pp;
    int havekids, pid;
    struct proc *p = myproc();

    acquire(&wait_lock);

    for (;;)
    {
        // Scan through table looking for exited children.
        havekids = 0;
        for (pp = proc; pp < &proc[NPROC]; pp++)
        {
            if (pp->parent == p)
            {
                // make sure the child isn't still in exit() or swtch().
                acquire(&pp->lock);

                havekids = 1;
                if (pp->state == ZOMBIE)
                {
                    // Found one.
                    pid = pp->pid;
                    if (addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate,
                                             sizeof(pp->xstate)) < 0)
                    {
                        release(&pp->lock);
                        release(&wait_lock);
                        return -1;
                    }
                    freeproc(pp);
                    release(&pp->lock);
                    release(&wait_lock);
                    return pid;
                }
                release(&pp->lock);
            }
        }

        // No point waiting if we don't have any children.
        if (!havekids || killed(p))
        {
            release(&wait_lock);
            return -1;
        }

        // Wait for a child to exit.
        sleep(p, &wait_lock); // DOC: wait-sleep
    }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void scheduler(void)
{
    printf("in scheduler\n");
    struct proc *p = myproc();
    printf("in scheduler after myproc: %p\n", p);
    struct cpu *c = mycpu();
    printf("my cpu context: %p\n", c->context);
    struct kthread *kt;
    printf("in scheduler after mycpu\n");
    c->kt = 0;
    printf("in scheduler before for\n");
    for (;;)
    {
        // Avoid deadlock by ensuring that devices can interrupt.
        intr_on();
        printf("in scheduler in first for\n");
        //acquire(&p->lock);
        for (kt = p->kthread; kt < &p->kthread[NKT]; kt++)
        {
            printf("in scheduler in second for\n");
            acquire(&kt->ktlock);
            if (kt->ktstate == KT_RUNNABLE)
            {
                printf("in scheduler in if\n");
                kt->ktstate = KT_RUNNING;                
                c->kt = kt;
                printf("c->context: %p\nc->kt->context: %p\n", c->context, c->kt->context);
                swtch(&c->context, &c->kt->context);
                c->kt = 0;
            }
            release(&kt->ktlock);
        }
        // if (p->state == RUNNABLE)
        // {
        //     // printf("in scheduler in for\n");
        //     //  Switch to chosen process.  It is the process's job
        //     //  to release its lock and then reacquire it
        //     //  before jumping back to us.
        //     p->state = RUNNING;
        //     // printf("in scheduler after RUNNING\n");
        //     c->p = p;
        //     // printf("in scheduler before swtch\n");
        //     swtch(c->context, c->p->context);

        //     // Process is done running for now.
        //     // It should have changed its p->state before coming back.
        //     c->p = 0;
        // }
        //release(&p->lock);
    }
}

// {
//     // printf("in scheduler\n");
//     struct proc *p;
//     struct cpu *c = mycpu();
//     struct thread *kt;
//     // printf("in scheduler after mycpu\n");
//     c->kt = 0;
//     // printf("in scheduler before for\n");
//     for (;;)
//     {
//         // Avoid deadlock by ensuring that devices can interrupt.
//         intr_on();
//         printf("in scheduler in for\n");
//         for (p = proc; p < &proc[NPROC]; p++)
//         {
//             acquire(&p->lock);
//             if (p->state == RUNNABLE)
//             {
//                 // printf("in scheduler in for\n");
//                 //  Switch to chosen process.  It is the process's job
//                 //  to release its lock and then reacquire it
//                 //  before jumping back to us.
//                 p->state = RUNNING;
//                 // printf("in scheduler after RUNNING\n");
//                 c->p = p;
//                 // printf("in scheduler before swtch\n");
//                 swtch(c->context, c->p->context);

//                 // Process is done running for now.
//                 // It should have changed its p->state before coming back.
//                 c->p = 0;
//             }
//             release(&p->lock);
//         }
//     }
// }

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void sched(void)
{
    printf("in sched\n");
    int intena;
    struct proc *p = myproc();

    if (!holding(&p->lock))
        panic("sched p->lock");
    if (mycpu()->noff != 1)
        panic("sched locks");
    if (p->kthread->ktstate == KT_RUNNING)
        panic("sched running");
    if (intr_get())
        panic("sched interruptible");

    intena = mycpu()->intena;
    swtch(&mykthread()->context, &mycpu()->context);
    mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void yield(void)
{
    printf("in yield\n");
    struct kthread *kt = mykthread();
    acquire(&kt->ktlock);
    kt->ktstate = KT_RUNNABLE;
    sched();
    release(&kt->ktlock);
}
// void yield(void)
// {
//     printf("in yield\n");
//     struct proc *p = myproc();
//     acquire(&p->lock);
//     p->state = RUNNABLE;
//     sched();
//     release(&p->lock);
// }


// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void forkret(void)
{
    printf("in forkret\n");
    static int first = 1;

    // Still holding p->lock from scheduler.
    release(&myproc()->lock);

    if (first)
    {
        // File system initialization must be run in the context of a
        // regular process (e.g., because it calls sleep), and thus cannot
        // be run from main().
        first = 0;
        fsinit(ROOTDEV);
    }

    usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void sleep(void *chan, struct spinlock *lk)
{
    printf("in sleep\n");
    struct kthread *kt = mykthread();

    // Must acquire p->lock in order to
    // change p->state and then call sched.
    // Once we hold p->lock, we can be
    // guaranteed that we won't miss any wakeup
    // (wakeup locks p->lock),
    // so it's okay to release lk.

    acquire(&kt->ktlock); // DOC: sleeplock1
    release(lk);

    // Go to sleep.
    kt->ktchan = chan;
    kt->ktstate = KT_SLEEPING;

    sched();

    // Tidy up.
    kt->ktchan = 0;

    // Reacquire original lock.
    release(&kt->ktlock);
    acquire(lk);
}

// void sleep(void *chan, struct spinlock *lk)
// {
//     printf("in sleep\n");
//     struct proc *p = myproc();

//     // Must acquire p->lock in order to
//     // change p->state and then call sched.
//     // Once we hold p->lock, we can be
//     // guaranteed that we won't miss any wakeup
//     // (wakeup locks p->lock),
//     // so it's okay to release lk.

//     acquire(&p->lock); // DOC: sleeplock1
//     release(lk);

//     // Go to sleep.
//     p->chan = chan;
//     p->state = SLEEPING;

//     sched();

//     // Tidy up.
//     p->chan = 0;

//     // Reacquire original lock.
//     release(&p->lock);
//     acquire(lk);
// }

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void wakeup(void *chan)
{
    printf("in wakeup\n");
    struct proc *p = myproc();
    struct kthread *kt;

    for (kt = p->kthread; kt < &p->kthread[NKT]; kt++)
    {
        printf("in wakeup in for\n");
        if (kt != mykthread())
        {
            printf("in wakeup in first if\n");
            acquire(&kt->ktlock);
            if (kt->ktstate == KT_SLEEPING && kt->ktchan == chan)
            {
                printf("in wakeup in second if\n");
                kt->ktstate = KT_RUNNABLE;
            }
            release(&kt->ktlock);
        }
    }
}
// void wakeup(void *chan)
// {
//     printf("in wakeup\n");
//     struct proc *p;

//     for (p = proc; p < &proc[NPROC]; p++)
//     {
//         printf("in wakeup in for\n");
//         if (p != myproc())
//         {
//             printf("in wakeup in first if\n");
//             acquire(&p->lock);
//             if (p->state == SLEEPING && p->chan == chan)
//             {
//                 printf("in wakeup in second if\n");
//                 p->state = RUNNABLE;
//             }
//             release(&p->lock);
//         }
//     }
// }

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int kill(int ktid)
{
    printf("in kill\n");
    struct proc *p = myproc();
    struct kthread *kt;

    for (kt = p->kthread; kt < &p->kthread[NKT]; kt++)
    {
        acquire(&kt->ktlock);
        if (kt->ktid == ktid)
        {
            kt->ktkilled = 1;
            if (kt->ktstate == KT_SLEEPING)
            {
                // Wake process from sleep().
                kt->ktstate = KT_RUNNABLE;
            }
            //release(&kt->lock);
            // task2
            // for (struct kthread *kt = p->kthread; kt < &p->kthread[NKT]; kt++)
            // {
            //     acquire(&p->kthread->ktlock);
            //     if (kt->ktstate == KT_SLEEPING)
            //     {
            //         kt->ktstate = KT_RUNNABLE;
            //     }
            //     release(&p->kthread->ktlock);
            // }

            return 0;
        }
        release(&kt->ktlock);
    }
    return -1;
}

// int kill(int pid)
// {
//     printf("in kill\n");
//     struct proc *p;

//     for (p = proc; p < &proc[NPROC]; p++)
//     {
//         acquire(&p->lock);
//         if (p->pid == pid)
//         {
//             p->killed = 1;
//             if (p->state == SLEEPING)
//             {
//                 // Wake process from sleep().
//                 p->state = RUNNABLE;
//             }
//             release(&p->lock);
//             // task2
//             for (struct kthread *kt = p->kthread; kt < &p->kthread[NKT]; kt++)
//             {
//                 acquire(&p->kthread->ktlock);
//                 if (kt->ktstate == KT_SLEEPING)
//                 {
//                     kt->ktstate = KT_RUNNABLE;
//                 }
//                 release(&p->kthread->ktlock);
//             }

//             return 0;
//         }
//         release(&p->lock);
//     }
//     return -1;
// }

void setkilled(struct proc *p)
{
    printf("in setkilled\n");
    acquire(&p->lock);
    p->killed = 1;
    release(&p->lock);
}

int killed(struct proc *p)
{
    printf("in killed\n");
    int k;

    acquire(&p->lock);
    k = p->killed;
    release(&p->lock);
    return k;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
    printf("in either_copyout\n");
    struct proc *p = myproc();
    if (user_dst)
    {
        return copyout(p->pagetable, dst, src, len);
    }
    else
    {
        memmove((char *)dst, src, len);
        return 0;
    }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
    printf("in either_copyin\n");
    struct proc *p = myproc();
    if (user_src)
    {
        return copyin(p->pagetable, dst, src, len);
    }
    else
    {
        memmove(dst, (char *)src, len);
        return 0;
    }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void procdump(void)
{
    printf("in procdump\n");
    static char *ktstates[] = {
        [KT_UNUSED] "unused",
        [KT_USED] "used",
        [KT_SLEEPING] "sleep ",
        [KT_RUNNABLE] "runble",
        [KT_RUNNING] "run   ",
        [KT_ZOMBIE] "zombie"};
    struct proc *p = myproc();
    struct kthread *kt;
    char *ktstate;

    printf("\n");
    for (kt = p->kthread; kt < &p->kthread[NKT]; kt++)
    {
        if (kt->ktstate == KT_UNUSED)
            continue;
        if (kt->ktstate >= 0 && kt->ktstate < NELEM(ktstates) && ktstates[kt->ktstate])
            ktstate = ktstates[kt->ktstate];
        else
            ktstate = "???";
        printf("%d %s", kt->ktid, ktstate);
        printf("\n");
    }
}
// void procdump(void)
// {
//     printf("in procdump\n");
//     static char *states[] = {
//         [UNUSED] "unused",
//         [USED] "used",
//         [SLEEPING] "sleep ",
//         [RUNNABLE] "runble",
//         [RUNNING] "run   ",
//         [ZOMBIE] "zombie"};
//     struct proc *p;
//     char *state;

//     printf("\n");
//     for (p = proc; p < &proc[NPROC]; p++)
//     {
//         if (p->state == UNUSED)
//             continue;
//         if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
//             state = states[p->state];
//         else
//             state = "???";
//         printf("%d %s %s", p->pid, state, p->name);
//         printf("\n");
//     }
// }
