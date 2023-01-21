
#include "types.h"
#include "globals.h"
#include "kernel.h"
#include "errno.h"

#include "util/debug.h"

#include "proc/proc.h"

#include "mm/mm.h"
#include "mm/mman.h"
#include "mm/page.h"
#include "mm/mmobj.h"
#include "mm/pframe.h"
#include "mm/pagetable.h"
#include "mm/tlb.h"

#include "vm/pagefault.h"
#include "vm/vmmap.h"

/*
 * This gets called by _pt_fault_handler in mm/pagetable.c The
 * calling function has already done a lot of error checking for
 * us. In particular it has checked that we are not page faulting
 * while in kernel mode. Make sure you understand why an
 * unexpected page fault in kernel mode is bad in Weenix. You
 * should probably read the _pt_fault_handler function to get a
 * sense of what it is doing.
 *
 * Before you can do anything you need to find the vmarea that
 * contains the address that was faulted on. Make sure to check
 * the permissions on the area to see if the process has
 * permission to do [cause]. If either of these checks does not
 * pass kill the offending process, setting its exit status to
 * EFAULT (normally we would send the SIGSEGV signal, however
 * Weenix does not support signals).
 *
 * Now it is time to find the correct page. Make sure that if the
 * user writes to the page it will be handled correctly. This
 * includes your shadow objects' copy-on-write magic working
 * correctly.
 *
 * Finally call pt_map to have the new mapping placed into the
 * appropriate page table.
 *
 * @param vaddr the address that was accessed to cause the fault
 *
 * @param cause this is the type of operation on the memory
 *              address which caused the fault, possible values
 *              can be found in pagefault.h
 */
void
handle_pagefault(uintptr_t vaddr, uint32_t cause)
{
        // NOT_YET_IMPLEMENTED("VM: handle_pagefault");

        // find pageFaultVmArea
        vmarea_t* pageFaultVmArea = vmmap_lookup(curproc->p_vmmap, ADDR_TO_PN(vaddr));
        // check pageFaultVmArea
        if(NULL == pageFaultVmArea){
                do_exit(EFAULT);
        }

        // check permission
        int needRead = !(cause & FAULT_WRITE || cause & FAULT_EXEC);
        int needWrite = cause & FAULT_WRITE;
        int needExec = cause & FAULT_EXEC;
        if(needRead){
                if(!(pageFaultVmArea->vma_prot & PROT_READ)){
                        do_exit(EFAULT);
                }
        }
        if(needWrite){
                if(!(pageFaultVmArea->vma_prot & PROT_WRITE)){
                        do_exit(EFAULT);
                }
        }
        if(needExec){
                if(!(pageFaultVmArea->vma_prot & PROT_EXEC)){
                        do_exit(EFAULT);
                }
        }

        // prepare parameter
        int forwrite = 0;
        int ptFlag = PT_PRESENT | PT_USER;
        int pdFlag = PD_PRESENT | PD_USER;
        if(needWrite){
                forwrite = 1;
                ptFlag |= PT_WRITE;
                pdFlag |= PD_WRITE;
        }

        // find pageFaultPageFrame
        pframe_t* pageFaultPageFrame = NULL;
        int code = pframe_lookup(pageFaultVmArea->vma_obj, ADDR_TO_PN(vaddr) - pageFaultVmArea->vma_start + pageFaultVmArea->vma_off, forwrite, &pageFaultPageFrame);
        // check pageFaultPageFrame
        if(code < 0){
                do_exit(EFAULT);
        }

        // this page frame must be non-NULL
        KASSERT(pageFaultPageFrame);
        // this page frame's pf_addr must be non-NULL
        KASSERT(pageFaultPageFrame->pf_addr);

        // if it is about to write
        if(1 == forwrite){
                // make pageFaultPageFrame dirty
                pframe_pin(pageFaultPageFrame);
                pframe_dirty(pageFaultPageFrame);
                pframe_unpin(pageFaultPageFrame);
        }

        // get physicalAddress
        uintptr_t physicalAddress = (uintptr_t) pt_virt_to_phys((uintptr_t) pageFaultPageFrame->pf_addr);
        // add pageEntry
        code = pt_map(curproc->p_pagedir, (uintptr_t) PAGE_ALIGN_DOWN(vaddr), physicalAddress, pdFlag, ptFlag);
        // update pageTable
        tlb_flush((uintptr_t) PAGE_ALIGN_DOWN(vaddr));
}
