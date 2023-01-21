
#include "globals.h"
#include "errno.h"
#include "types.h"

#include "mm/mm.h"
#include "mm/tlb.h"
#include "mm/mman.h"
#include "mm/page.h"

#include "proc/proc.h"

#include "util/string.h"
#include "util/debug.h"

#include "fs/vnode.h"
#include "fs/vfs.h"
#include "fs/file.h"

#include "vm/vmmap.h"
#include "vm/mmap.h"

/*
 * This function implements the mmap(2) syscall, but only
 * supports the MAP_SHARED, MAP_PRIVATE, MAP_FIXED, and
 * MAP_ANON flags.
 *
 * Add a mapping to the current process's address space.
 * You need to do some error checking; see the ERRORS section
 * of the manpage for the problems you should anticipate.
 * After error checking most of the work of this function is
 * done by vmmap_map(), but remember to clear the TLB.
 */
int
do_mmap(void *addr, size_t len, int prot, int flags,
        int fd, off_t off, void **ret)
{
        // NOT_YET_IMPLEMENTED("VM: do_mmap");

        uint32_t lopage;
        vnode_t *vn = NULL;
        file_t *file = NULL;
        uint32_t npages;
        vmarea_t *new;
        int err;

        /* do some error checking */ 
        // err. off and addr(if not NULL) should be page aligned
        if(!PAGE_ALIGNED(off) || (addr != NULL && !PAGE_ALIGNED(addr))){
                return -EINVAL;
        }
        // err. len should be greater than 0, and the endaddr should not exceed user space
        if((uint32_t)addr + len > USER_MEM_HIGH || len == 0){
                return -EINVAL;
        }
        // err. flag should be either PRIVATE or SHARED
        if(!(flags & MAP_PRIVATE) && !(flags & MAP_SHARED)){
                return -EINVAL;
        }
        // err. when flag is FIXED, addr cannot be null
        if(flags & MAP_FIXED && addr == NULL){
                return -EINVAL;
        }

        // init lopage for vmmap_map
        lopage = (addr == NULL) ? 0 : ADDR_TO_PN(addr);

        // init file if it is not MAP_ANON
        if(!(flags & MAP_ANON)){
                // remember to close!
                if(fd == -1 || (file = fget(fd)) == NULL){
                        return -EBADF;
                }
                vn = file->f_vnode;
                // err. file access not permitted, i.e. write on not-copy-on-write
                if(!(file->f_mode & FMODE_WRITE) && (prot & PROT_WRITE) && !(flags & MAP_PRIVATE)){
                        fput(file);
                        return -EACCES;
                }
        }

        // calculate npages
        npages = len / PAGE_SIZE;
        if (len % PAGE_SIZE != 0){
                npages++;
        }

        // call vmmap_map
        if((err = vmmap_map(curproc->p_vmmap, vn, lopage, npages, prot, flags, off, VMMAP_DIR_HILO, &new)) < 0){
                if(file != NULL){
                        fput(file);
                }
                return err;
        }

        // set ret
        *ret = PN_TO_ADDR(new->vma_start);

        // remember to clear the TLB, and close file
        tlb_flush_all();
        if(file != NULL){
                fput(file);
        }

        // page table must be valid after a memory segment is mapped into the address space
        KASSERT(NULL != curproc->p_pagedir);

        return 0;
}


/*
 * This function implements the munmap(2) syscall.
 *
 * As with do_mmap() it should perform the required error checking,
 * before calling upon vmmap_remove() to do most of the work.
 * Remember to clear the TLB.
 */
int
do_munmap(void *addr, size_t len)
{
        // NOT_YET_IMPLEMENTED("VM: do_munmap");

        // do some error checking
        // err. addr should be page aligned
        if(!PAGE_ALIGNED(addr)){
                return -EINVAL;
        }
        // err. the endaddr should not exceed user space, and len should be greater than 0
        if((uint32_t)addr + len > USER_MEM_HIGH || (uint32_t)addr + len < USER_MEM_LOW || len == 0 || len > USER_MEM_HIGH){
                return -EINVAL;
        }

        // calculate npages
        uint32_t npages = len / PAGE_SIZE + ((len % PAGE_SIZE == 0) ? 0 : 1);

        // call vmmap_remove()
        vmmap_remove(curproc->p_vmmap, ADDR_TO_PN(addr), npages);

        // remember to clear the TLB
        tlb_flush_all();

        return 0;
}

