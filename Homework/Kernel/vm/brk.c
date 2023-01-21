
#include "globals.h"
#include "errno.h"
#include "util/debug.h"

#include "mm/mm.h"
#include "mm/page.h"
#include "mm/mman.h"

#include "vm/mmap.h"
#include "vm/vmmap.h"

#include "proc/proc.h"

/*
 * This function implements the brk(2) system call.
 *
 * This routine manages the calling process's "break" -- the ending address
 * of the process's "dynamic" region (often also referred to as the "heap").
 * The current value of a process's break is maintained in the 'p_brk' member
 * of the proc_t structure that represents the process in question.
 *
 * The 'p_brk' and 'p_start_brk' members of a proc_t struct are initialized
 * by the loader. 'p_start_brk' is subsequently never modified; it always
 * holds the initial value of the break. Note that the starting break is
 * not necessarily page aligned!
 *
 * 'p_start_brk' is the lower limit of 'p_brk' (that is, setting the break
 * to any value less than 'p_start_brk' should be disallowed).
 *
 * The upper limit of 'p_brk' is defined by the minimum of (1) the
 * starting address of the next occuring mapping or (2) USER_MEM_HIGH.
 * That is, growth of the process break is limited only in that it cannot
 * overlap with/expand into an existing mapping or beyond the region of
 * the address space allocated for use by userland. (note the presence of
 * the 'vmmap_is_range_empty' function).
 *
 * The dynamic region should always be represented by at most ONE vmarea.
 * Note that vmareas only have page granularity, you will need to take this
 * into account when deciding how to set the mappings if p_brk or p_start_brk
 * is not page aligned.
 *
 * You are guaranteed that the process data/bss region is non-empty.
 * That is, if the starting brk is not page-aligned, its page has
 * read/write permissions.
 *
 * If addr is NULL, you should "return" the current break. We use this to
 * implement sbrk(0) without writing a separate syscall. Look in
 * user/libc/syscall.c if you're curious.
 *
 * You should support combined use of brk and mmap in the same process.
 *
 * Note that this function "returns" the new break through the "ret" argument.
 * Return 0 on success, -errno on failure.
 */
int
do_brk(void *addr, void **ret)
{
        // NOT_YET_IMPLEMENTED("VM: do_brk");

        vmarea_t *vma;
        uint32_t prev_endvfn;
        uint32_t new_endvfn;
        uint32_t npages;

        if(addr == NULL){
                *ret = curproc->p_brk;
                return 0;
        }

        // check valid addr -- addr can be not page aligned
        if(addr < curproc->p_start_brk || (uint32_t)addr > USER_MEM_HIGH){
                return -ENOMEM;
        }

        // find the vmarea
        vma = vmmap_lookup(curproc->p_vmmap, ADDR_TO_PN(curproc->p_start_brk));
        
        prev_endvfn = vma->vma_end;
        new_endvfn = ADDR_TO_PN(PAGE_ALIGN_UP(addr));
        npages = new_endvfn - prev_endvfn;

        // case1: shorten the brk, simply call remove
        if(prev_endvfn > new_endvfn){
                vmmap_remove(curproc->p_vmmap, new_endvfn, -npages);
        }
        
        // case2: increase the brk, we need to check valid addr again if we need to alloc another page
        else if( prev_endvfn < new_endvfn){
                if(!vmmap_is_range_empty(curproc->p_vmmap, prev_endvfn, npages)){
                        return -ENOMEM;
                }

                vma->vma_end = new_endvfn;
        }

        // in both cases, set brk and ret
        curproc->p_brk = addr;
        *ret = curproc->p_brk;

        return 0;
}

/**
 * Please note:
 *      before you start testing /usr/bin/forkbomb or /usr/bin/stress, 
 * you should add all SELF-checks and follow grade guideline instruction
 * (i.e. DBG=error,print,test in Config.mk), otherwise it may have weird
 * pagefault(due to overflow) issue.
*/