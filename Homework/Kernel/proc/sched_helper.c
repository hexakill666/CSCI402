
#include "globals.h"
#include "errno.h"

#include "main/interrupt.h"

#include "proc/sched.h"
#include "proc/kthread.h"

#include "util/init.h"
#include "util/debug.h"

void ktqueue_enqueue(ktqueue_t *q, kthread_t *thr);
kthread_t * ktqueue_dequeue(ktqueue_t *q);

/*
 * Updates the thread's state and enqueues it on the given
 * queue. Returns when the thread has been woken up with wakeup_on or
 * broadcast_on.
 *
 * Use the private queue manipulation functions above.
 */
void
sched_sleep_on(ktqueue_t *q)
{       
        // NOT_YET_IMPLEMENTED("PROCS: sched_sleep_on");

        // curthr and q should not be NULL
        KASSERT(NULL != curthr || NULL != q);

        // add curthr to q
        curthr->kt_state = KT_SLEEP;
        ktqueue_enqueue(q, curthr);
        curthr->kt_wchan = q;
        
        // swtich context
        sched_switch();
}

kthread_t *
sched_wakeup_on(ktqueue_t *q)
{
        // NOT_YET_IMPLEMENTED("PROCS: sched_wakeup_on");

        // curthr and q should not be NULL, and q should not be empty
        KASSERT(NULL != curthr || NULL != q);
        KASSERT(!sched_queue_empty(q));

        // get wakeupThread from q
        kthread_t* wakeupThread = ktqueue_dequeue(q);
        
        // thr must be in either one of these two states
        KASSERT((wakeupThread->kt_state == KT_SLEEP) || (wakeupThread->kt_state == KT_SLEEP_CANCELLABLE));

        // add wakeupThread to runq, and change its state
        wakeupThread->kt_state = KT_RUN;
        sched_make_runnable(wakeupThread);
        
        return wakeupThread;
}

void
sched_broadcast_on(ktqueue_t *q)
{
        // NOT_YET_IMPLEMENTED("PROCS: sched_broadcast_on");

        // curthr and q should not be NULL
        KASSERT(NULL != curthr || NULL != q);
        
        // wake up one by one
        while(!sched_queue_empty(q)){
                sched_wakeup_on(q);
        }
}

