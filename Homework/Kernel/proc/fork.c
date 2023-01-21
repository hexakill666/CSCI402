
#include "types.h"
#include "globals.h"
#include "errno.h"

#include "util/debug.h"
#include "util/string.h"

#include "proc/proc.h"
#include "proc/kthread.h"

#include "mm/mm.h"
#include "mm/mman.h"
#include "mm/page.h"
#include "mm/pframe.h"
#include "mm/mmobj.h"
#include "mm/pagetable.h"
#include "mm/tlb.h"

#include "fs/file.h"
#include "fs/vnode.h"

#include "vm/shadow.h"
#include "vm/vmmap.h"

#include "api/exec.h"

#include "main/interrupt.h"

/* Pushes the appropriate things onto the kernel stack of a newly forked thread
 * so that it can begin execution in userland_entry.
 * regs: registers the new thread should have on execution
 * kstack: location of the new thread's kernel stack
 * Returns the new stack pointer on success. */
static uint32_t
fork_setup_stack(const regs_t *regs, void *kstack)
{
        /* Pointer argument and dummy return address, and userland dummy return
         * address */
        uint32_t esp = ((uint32_t) kstack) + DEFAULT_STACK_SIZE - (sizeof(regs_t) + 12);
        *(void **)(esp + 4) = (void *)(esp + 8); /* Set the argument to point to location of struct on stack */
        memcpy((void *)(esp + 8), regs, sizeof(regs_t)); /* Copy over struct */
        return esp;
}


/*
 * The implementation of fork(2). Once this works,
 * you're practically home free. This is what the
 * entirety of Weenix has been leading up to.
 * Go forth and conquer.
 */

/**
 * Allocate a proc_t out of the procs structure using proc_create().
 * Copy the vmmap_t from the parent process into the child using vmmap_clone(). Remember to increase the reference counts on the underlying mmobj_ts.
 * For each private mapping, point the vmarea_t at the new shadow object, which in turn should point to the original mmobj_t for the vmarea_t. This is how you know that the pages corresponding to this mapping are copy-on-write. Be careful with reference counts. Also note that for shared mappings, there is no need to copy the mmobj_t.
 * Unmap the user land page table entries and flush the TLB (using pt_unmap_range() and tlb_flush_all()). This is necessary because the parent process might still have some entries marked as "writable", but since we are implementing copy-on-write we would like access to these pages to cause a trap.
 * Set up the new process thread context (kt_ctx). You will need to set the following:
 *         c_pdptr - the page table pointer
 *         c_eip - function pointer for the userland_entry() function
 *         c_esp - the value returned by fork_setup_stack()
 *         c_kstack - the top of the new thread's kernel stack
 *         c_kstacksz - size of the new thread's kernel stack
 * Remember to set the return value in the child process!
 * Copy the file descriptor table of the parent into the child. Remember to use fref() here.
 * Set the child's working directory to point to the parent's working directory (once again, remember reference counts).
 * Use kthread_clone() to copy the thread from the parent process into the child process.
 * Set any other fields in the new process which need to be set.
 * Make the new thread runnable.
*/
int
do_fork(struct regs *regs)
{
        // NOT_YET_IMPLEMENTED("VM: do_fork");

        // the function argument must be non-NULL
        KASSERT(regs != NULL);
        // the parent process, which is curproc, must be non-NULL
        KASSERT(curproc != NULL);
        // the parent process must be in the running state and not in the zombie state
        KASSERT(curproc->p_state == PROC_RUNNING);

        // Allocate a proc_t out of the procs structure using proc_create().
        proc_t* childProcess = proc_create("childProcess");

        // Copy the vmmap_t from the parent process into the child using vmmap_clone(). 
        vmmap_t* childVmMap = vmmap_clone(curproc->p_vmmap);
        childVmMap->vmm_proc = childProcess;
        childProcess->p_vmmap = childVmMap;

        // Remember to increase the reference counts on the underlying mmobj_ts.
        // For each private mapping, point the vmarea_t at the new shadow object, 
        // which in turn should point to the original mmobj_t for the vmarea_t. 
        // This is how you know that the pages corresponding to this mapping are 
        // copy-on-write. Be careful with reference counts. 
        // Also note that for shared mappings, there is no need to copy the mmobj_t.
        vmarea_t* childVmArea = NULL;
        list_iterate_begin(&childProcess->p_vmmap->vmm_list, childVmArea, vmarea_t, vma_plink)
        {
                // find the corresponding vmarea in parent process
                vmarea_t* parentVmArea = vmmap_lookup(curproc->p_vmmap, childVmArea->vma_start);
                // if share mapping
                if(MAP_SHARED == (childVmArea->vma_flags & MAP_TYPE)){
                        // child and parent only need to point to the same mmobj
                        childVmArea->vma_obj = parentVmArea->vma_obj;
                        // increase refcount
                        childVmArea->vma_obj->mmo_ops->ref(childVmArea->vma_obj);
                }
                // if private mapping
                else{
                        // both child and parent need a new shadow mmobj
                        // both these two new shadow mmobj point to the old parent mmobj
                        // also need to increase refcount of the old parent mmobj and the bottom mmobj
                        mmobj_t* parentShadow = shadow_create();
                        parentShadow->mmo_shadowed = parentVmArea->vma_obj;
                        parentShadow->mmo_un.mmo_bottom_obj = mmobj_bottom_obj(parentVmArea->vma_obj);
                        
                        mmobj_t* childShadow = shadow_create();
                        childShadow->mmo_shadowed = parentVmArea->vma_obj;
                        childShadow->mmo_un.mmo_bottom_obj = mmobj_bottom_obj(parentVmArea->vma_obj);

                        childShadow->mmo_shadowed->mmo_ops->ref(childShadow->mmo_shadowed);
                        childShadow->mmo_un.mmo_bottom_obj->mmo_ops->ref(childShadow->mmo_un.mmo_bottom_obj);
                        parentShadow->mmo_un.mmo_bottom_obj->mmo_ops->ref(parentShadow->mmo_un.mmo_bottom_obj);
                        
                        parentVmArea->vma_obj = parentShadow;
                        childVmArea->vma_obj = childShadow;
                }
                // for the old parent mmobj's bottom mmobj, add childVmArea to mmo_vmas
                list_insert_tail(mmobj_bottom_vmas(parentVmArea->vma_obj), &childVmArea->vma_olink);
        }
        list_iterate_end();

        // Unmap the user land page table entries and flush the TLB (using pt_unmap_range() and tlb_flush_all()). 
        // This is necessary because the parent process might still have some entries marked as "writable", 
        // but since we are implementing copy-on-write we would like access to these pages to cause a trap.
        pt_unmap_range(curproc->p_pagedir, USER_MEM_LOW, USER_MEM_HIGH);
        tlb_flush_all();

        // Set up the new process thread context (kt_ctx).
        kthread_t* childThread = kthread_clone(curthr);

        // new child process starts in the running state
        KASSERT(childProcess->p_state == PROC_RUNNING); 
        // new child process must have a valid page table
        KASSERT(childProcess->p_pagedir != NULL);
        // thread in the new child process must have a valid kernel stack
        KASSERT(childThread->kt_kstack != NULL);

        // set eax
        regs->r_eax = 0;

        // c_pdptr - the page table pointer
        childThread->kt_ctx.c_pdptr = childProcess->p_pagedir;
        // c_eip - function pointer for the userland_entry() function
        childThread->kt_ctx.c_eip = (uint32_t) userland_entry;
        // c_esp - the value returned by fork_setup_stack()
        childThread->kt_ctx.c_esp = fork_setup_stack(regs, childThread->kt_kstack);
        // c_kstack - the top of the new thread's kernel stack
        childThread->kt_ctx.c_kstack = (uintptr_t) childThread->kt_kstack;
        // c_kstacksz - size of the new thread's kernel stack
        childThread->kt_ctx.c_kstacksz = DEFAULT_STACK_SIZE;
        
        // Remember to set the return value in the child process!
        regs->r_eax = childProcess->p_pid;

        // Copy the file descriptor table of the parent into the child. 
        // Remember to use fref() here.
        for(int a = 0; a < NFILES; a++){
                childProcess->p_files[a] = curproc->p_files[a];
                if(NULL != childProcess->p_files[a]){
                        fref(childProcess->p_files[a]);
                }
        }

        // Set the child's working directory to point to the parent's working directory 
        // (once again, remember reference counts).
        if(NULL != childProcess->p_cwd){
                vput(childProcess->p_cwd);
        }
        childProcess->p_cwd = curproc->p_cwd;
        if(NULL != childProcess->p_cwd){
                vref(childProcess->p_cwd);
        }

        // Use kthread_clone() to copy the thread from the parent process into the child process.
        // add childThread to childProcess
        childThread->kt_proc = childProcess;
        list_insert_tail(&childProcess->p_threads, &childThread->kt_plink);

        // Set any other fields in the new process which need to be set.
        childProcess->p_brk = curproc->p_brk;
        childProcess->p_start_brk = curproc->p_start_brk;

        // Make the new thread runnable.
        sched_make_runnable(childThread);

        return childProcess->p_pid;
}
