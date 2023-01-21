
#include "globals.h"
#include "errno.h"

#include "util/debug.h"

#include "proc/kthread.h"
#include "proc/kmutex.h"

/*
 * IMPORTANT: Mutexes can _NEVER_ be locked or unlocked from an
 * interrupt context. Mutexes are _ONLY_ lock or unlocked from a
 * thread context.
 */

void
kmutex_init(kmutex_t *mtx)
{
        // NOT_YET_IMPLEMENTED("PROCS: kmutex_init");

        // mtx should not be NULL
        KASSERT(NULL != mtx);

        // fill in attribute
        sched_queue_init(&mtx->km_waitq);
        mtx->km_holder = NULL;
}

/*
 * This should block the current thread (by sleeping on the mutex's
 * wait queue) if the mutex is already taken.
 *
 * No thread should ever try to lock a mutex it already has locked.
 */
void
kmutex_lock(kmutex_t *mtx)
{
        // NOT_YET_IMPLEMENTED("PROCS: kmutex_lock");

        // mtx shouldn't be NULL
        KASSERT(NULL != mtx);
        // curthr must be valid and it must not be holding the mutex (mtx) already
        KASSERT(curthr && (curthr != mtx->km_holder)); 

        // if mtx hasn't been locked, get the mtx lock
        if(NULL == mtx->km_holder){
                mtx->km_holder = curthr;
        }
        // if mtx has been locked
        else{
                // add curthr to waitq
                sched_sleep_on(&mtx->km_waitq);
        }
}

/*
 * This should do the same as kmutex_lock, but use a cancellable sleep
 * instead. Also, if you are cancelled while holding mtx, you should unlock mtx.
 */
int
kmutex_lock_cancellable(kmutex_t *mtx)
{
        // NOT_YET_IMPLEMENTED("PROCS: kmutex_lock_cancellable");

        // mtx shouldn't be NULL
        KASSERT(NULL != mtx);
        // curthr must be valid and it must not be holding the mutex (mtx) already
        KASSERT(curthr && (curthr != mtx->km_holder)); 

        // if curthr has been cancelled
        if(curthr->kt_cancelled == 1){
                return -EINTR;
        } 
        
        // if mtx hasn't been locked
        if(NULL == mtx->km_holder){
                mtx->km_holder = curthr;
        }
        // if mtx has been locked
        else{
                // add curthr to waitq
                int code = sched_cancellable_sleep_on(&mtx->km_waitq);
                // if code == 0, curthr has been waked up and hasn't been cancelled
                // if curthr has been cancelled
                if(code == -EINTR){
                        // if curthr has been cancelled and holds mtx lock
                        if(mtx->km_holder == curthr){
                                // unlock mtx
                                kmutex_unlock(mtx);
                        }
                        // no matter curthr holds mtx lock or not,
                        // as long as curthr has been cancelled, return -EINTR
                        return code;
                }
        }

        return 0;
}

/*
 * If there are any threads waiting to take a lock on the mutex, one
 * should be woken up and given the lock.
 *
 * Note: This should _NOT_ be a blocking operation!
 *
 * Note: Ensure the new owner of the mutex enters the run queue.
 *
 * Note: Make sure that the thread on the head of the mutex's wait
 * queue becomes the new owner of the mutex.
 *
 * @param mtx the mutex to unlock
 */
void
kmutex_unlock(kmutex_t *mtx)
{
        // NOT_YET_IMPLEMENTED("PROCS: kmutex_unlock");

        // mtx shouldn't be NULL
        KASSERT(NULL != mtx);
        // curthr must be valid and it must currently holding the mutex (mtx)
        KASSERT(curthr && (curthr == mtx->km_holder));

        // unlock mtx
        mtx->km_holder = NULL;
        // if waitq is not empty
        if(!sched_queue_empty(&mtx->km_waitq)){
                // wake up nextThread
                kthread_t* nextThread = sched_wakeup_on(&mtx->km_waitq);
                // nextThread become mtx holder
                mtx->km_holder = nextThread;
        }

        // on return, curthr must not be the mutex (mtx) holder
        KASSERT(curthr != mtx->km_holder);
}
