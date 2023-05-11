
#include "kernel/types.h"
#include "uthread.h"
#include "user.h"
#include "kernel/syscall.h"

struct uthread utheads[MAX_UTHREADS];
struct uthread *currThread = 0 ;

int uthread_create(void (*start_func)(), enum sched_priority priority){
    int freeIdx = -1;
    for(int i=0; i<MAX_UTHREADS; i++){ //find free place
        if(utheads[i].state==FREE){
            freeIdx=i;
            break;
        }
    }
    if(freeIdx != -1){// check if there is place to add new user thread
        struct uthread *uth = &utheads[freeIdx];
        // add uthread fields
        uth->priority=priority;
        uth->context.ra=(uint64) start_func;
        uth->context.sp=(uint64) (uth->ustack+ STACK_SIZE);//check it 
        uth->state = RUNNABLE;
    }else{
        return freeIdx;
    }
    return 0 ;
}
struct uthread* getNextUthread();

void uthread_yield(){
    struct uthread *oldUthread = currThread;
    struct uthread *nextUthread = getNextUthread();
    if(nextUthread){
        currThread = nextUthread;
        uswtch(&oldUthread->context,&nextUthread->context);
        oldUthread->state= RUNNABLE;//chech this
    }
}
struct uthread* getNextUthread(){
    struct uthread *nestUthread =0;
    int maxPiority = -1;
    for(int i=0; i<MAX_UTHREADS;i++){
        if(utheads[i].priority> maxPiority && utheads[i].state == RUNNABLE){
            maxPiority =utheads[i].priority;
            nestUthread = &utheads[i];
        }
    }
    return nestUthread;
}
void checkIfAllFree(){
    for(int i=0;i<MAX_UTHREADS;i++){
        if(utheads[i].state != FREE){
            return;
        }
    }
    exit(0);//SYS_exit;
}

void uthread_exit(){
    if(currThread){
        currThread->state=FREE;
        uthread_yield();
    }
    checkIfAllFree();
}

int uthread_start_all(){
    uthread_yield();
    return -1;

}
enum sched_priority uthread_set_priority(enum sched_priority priority){
    if(currThread){
        enum sched_priority oldPriority = currThread->priority;
        currThread->priority = priority;
        return oldPriority;
    }
    return -1;

}
enum sched_priority uthread_get_priority(){
    if(currThread){
        return currThread->priority;
    }
    return -1;
}

struct uthread* uthread_self(){
    return currThread;
}