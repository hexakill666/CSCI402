
#include "kernel.h"
#include "errno.h"
#include "globals.h"

#include "vm/vmmap.h"
#include "vm/shadow.h"
#include "vm/anon.h"

#include "proc/proc.h"

#include "util/debug.h"
#include "util/list.h"
#include "util/string.h"
#include "util/printf.h"

#include "fs/vnode.h"
#include "fs/file.h"
#include "fs/fcntl.h"
#include "fs/vfs_syscall.h"

#include "mm/slab.h"
#include "mm/page.h"
#include "mm/mm.h"
#include "mm/mman.h"
#include "mm/mmobj.h"
#include "mm/tlb.h"

static slab_allocator_t *vmmap_allocator;
static slab_allocator_t *vmarea_allocator;

void
vmmap_init(void)
{
        vmmap_allocator = slab_allocator_create("vmmap", sizeof(vmmap_t));
        KASSERT(NULL != vmmap_allocator && "failed to create vmmap allocator!");
        vmarea_allocator = slab_allocator_create("vmarea", sizeof(vmarea_t));
        KASSERT(NULL != vmarea_allocator && "failed to create vmarea allocator!");
}

vmarea_t *
vmarea_alloc(void)
{
        vmarea_t *newvma = (vmarea_t *) slab_obj_alloc(vmarea_allocator);
        if (newvma) {
                newvma->vma_vmmap = NULL;
        }
        return newvma;
}

void
vmarea_free(vmarea_t *vma)
{
        KASSERT(NULL != vma);
        slab_obj_free(vmarea_allocator, vma);
}

/* a debugging routine: dumps the mappings of the given address space. */
size_t
vmmap_mapping_info(const void *vmmap, char *buf, size_t osize)
{
        KASSERT(0 < osize);
        KASSERT(NULL != buf);
        KASSERT(NULL != vmmap);

        vmmap_t *map = (vmmap_t *)vmmap;
        vmarea_t *vma;
        ssize_t size = (ssize_t)osize;

        int len = snprintf(buf, size, "%21s %5s %7s %8s %10s %12s\n",
                           "VADDR RANGE", "PROT", "FLAGS", "MMOBJ", "OFFSET",
                           "VFN RANGE");

        list_iterate_begin(&map->vmm_list, vma, vmarea_t, vma_plink) {
                size -= len;
                buf += len;
                if (0 >= size) {
                        goto end;
                }

                len = snprintf(buf, size,
                               "%#.8x-%#.8x  %c%c%c  %7s 0x%p %#.5x %#.5x-%#.5x\n",
                               vma->vma_start << PAGE_SHIFT,
                               vma->vma_end << PAGE_SHIFT,
                               (vma->vma_prot & PROT_READ ? 'r' : '-'),
                               (vma->vma_prot & PROT_WRITE ? 'w' : '-'),
                               (vma->vma_prot & PROT_EXEC ? 'x' : '-'),
                               (vma->vma_flags & MAP_SHARED ? " SHARED" : "PRIVATE"),
                               vma->vma_obj, vma->vma_off, vma->vma_start, vma->vma_end);
        } list_iterate_end();

end:
        if (size <= 0) {
                size = osize;
                buf[osize - 1] = '\0';
        }
        /*
        KASSERT(0 <= size);
        if (0 == size) {
                size++;
                buf--;
                buf[0] = '\0';
        }
        */
        return osize - size;
}

/* Create a new vmmap, which has no vmareas and does
 * not refer to a process. */
vmmap_t *
vmmap_create(void)
{
        // NOT_YET_IMPLEMENTED("VM: vmmap_create");

        vmmap_t *map = (vmmap_t *) slab_obj_alloc(vmmap_allocator);

        list_init(&map->vmm_list);
        map->vmm_proc = NULL;

        return map;
}

/* Removes all vmareas from the address space and frees the
 * vmmap struct. */
void
vmmap_destroy(vmmap_t *map)
{
        // NOT_YET_IMPLEMENTED("VM: vmmap_destroy");

        // function argument must not be NULL
        KASSERT(NULL != map);

        // Removes all vmareas from the address space
        vmarea_t *vma;
        list_iterate_begin(&map->vmm_list, vma, vmarea_t, vma_plink) 
        {
                /* do some cleanup:
                 * 1. remove the links
                 * 2. don't forget to put the vma_obj
                 * 3. free the vmarea*/
                list_remove(&vma->vma_plink);
                list_remove(&vma->vma_olink);
                vma->vma_obj->mmo_ops->put(vma->vma_obj);
                vmarea_free(vma);
        }
        list_iterate_end();

        // frees the vmmap struct
        slab_obj_free(vmmap_allocator, map);
}

/* Add a vmarea to an address space. Assumes (i.e. asserts to some extent)
 * the vmarea is valid.  This involves finding where to put it in the list
 * of VM areas, and adding it. Don't forget to set the vma_vmmap for the
 * area. */
void
vmmap_insert(vmmap_t *map, vmarea_t *newvma)
{
        // NOT_YET_IMPLEMENTED("VM: vmmap_insert");

        // both function arguments must not be NULL
        KASSERT(NULL != map && NULL != newvma);
        // newvma must be newly create and must not be part of any existing vmmap
        KASSERT(NULL == newvma->vma_vmmap);
        // newvma must not be empty
        KASSERT(newvma->vma_start < newvma->vma_end);
        // addresses in this memory segment must lie completely within the user space
        KASSERT(ADDR_TO_PN(USER_MEM_LOW) <= newvma->vma_start && ADDR_TO_PN(USER_MEM_HIGH) >= newvma->vma_end);

        // don't forget to set the vma_vmmap for the area
        newvma->vma_vmmap = map;
        vmarea_t *vma;
        list_iterate_begin(&map->vmm_list, vma, vmarea_t, vma_plink)
        {
                /* iterate the list to find the first vma whose address space 
                 * starts after the end of newvma, always remember the range
                 * is [start, end)*/
                if(vma->vma_start >= newvma->vma_end){
                        list_insert_before(&vma->vma_plink, &newvma->vma_plink);
                        return;
                }
        }
        list_iterate_end();

        // if the start address of newvma is the biggest, put it to the tail
        list_insert_tail(&map->vmm_list, &newvma->vma_plink);
}

/* Find a contiguous range of free virtual pages of length npages in
 * the given address space. Returns starting vfn for the range,
 * without altering the map. Returns -1 if no such range exists.
 *
 * Your algorithm should be first fit. If dir is VMMAP_DIR_HILO, you
 * should find a gap as high in the address space as possible; if dir
 * is VMMAP_DIR_LOHI, the gap should be as low as possible. */
int
vmmap_find_range(vmmap_t *map, uint32_t npages, int dir)
{
        // NOT_YET_IMPLEMENTED("VM: vmmap_find_range");

        vmarea_t *vma;
        uint32_t pre_start = ADDR_TO_PN(USER_MEM_HIGH);
        uint32_t pre_end = ADDR_TO_PN(USER_MEM_LOW);

        // as high in the address space as possible
        if(dir == VMMAP_DIR_HILO){
                list_iterate_reverse(&map->vmm_list, vma, vmarea_t, vma_plink)
                {
                        if(pre_start - vma->vma_end >= npages){
                                return pre_start - npages;
                        }
                        pre_start = vma->vma_start;
                }
                list_iterate_end();

                // don't forget to check the area before list_head
                vmarea_t *head_vma = list_head(&map->vmm_list, vmarea_t, vma_plink);
                if(head_vma->vma_start - ADDR_TO_PN(USER_MEM_LOW) >= npages){      
                        return head_vma->vma_start - npages;
                }
        }
        // as low in the address space as possible
        else{
                list_iterate_begin(&map->vmm_list, vma, vmarea_t, vma_plink)
                {
                        if(pre_end > 0 && vma->vma_start - pre_end >= npages){
                                return pre_end;
                        }
                        pre_end = vma->vma_end;
                }
                list_iterate_end();

                // don't forget to check the area after list_tail
                vmarea_t *tail_vma = list_tail(&map->vmm_list, vmarea_t, vma_plink);
                if(ADDR_TO_PN(USER_MEM_HIGH) - tail_vma->vma_end >= npages){
                        return tail_vma->vma_end;
                }
        }

        return -1;
}

/* Find the vm_area that vfn lies in. Simply scan the address space
 * looking for a vma whose range covers vfn. If the page is unmapped,
 * return NULL. */
vmarea_t *
vmmap_lookup(vmmap_t *map, uint32_t vfn)
{
        // NOT_YET_IMPLEMENTED("VM: vmmap_lookup");

        // the first function argument must not be NULL
        KASSERT(NULL != map); 

        vmarea_t *vma;
        list_iterate_begin(&map->vmm_list, vma, vmarea_t, vma_plink)
        {
                // [start, end)
                if(vma->vma_end > vfn && vma->vma_start <= vfn){
                        return vma;
                }
        }
        list_iterate_end();

        return NULL;
}

/* Allocates a new vmmap containing a new vmarea for each area in the
 * given map. The areas should have no mmobjs set yet. Returns pointer
 * to the new vmmap on success, NULL on failure. This function is
 * called when implementing fork(2). */
vmmap_t *
vmmap_clone(vmmap_t *map)
{
        // NOT_YET_IMPLEMENTED("VM: vmmap_clone");

        // create a new map and init it
        vmmap_t *newmap;
        newmap = vmmap_create();
        newmap->vmm_proc = map->vmm_proc;
        list_init(&newmap->vmm_list);

        vmarea_t *vma;
        list_iterate_begin(&map->vmm_list, vma, vmarea_t, vma_plink)
        {
                // create a new vma and insert it into the tail of newmap->vmm_list
                vmarea_t *newvma = vmarea_alloc();
                newvma->vma_end = vma->vma_end;
                newvma->vma_start = vma->vma_start;
                newvma->vma_flags = vma->vma_flags;
                newvma->vma_off = vma->vma_off;
                newvma->vma_prot = vma->vma_prot;
                newvma->vma_obj = NULL;
                list_init(&newvma->vma_plink);
                list_init(&newvma->vma_olink);
                newvma->vma_vmmap = newmap;
                list_insert_tail(&newmap->vmm_list, &newvma->vma_plink);
        }
        list_iterate_end();
        
        return newmap;
}

/* Insert a mapping into the map starting at lopage for npages pages.
 * If lopage is zero, we will find a range of virtual addresses in the
 * process that is big enough, by using vmmap_find_range with the same
 * dir argument.  If lopage is non-zero and the specified region
 * contains another mapping that mapping should be unmapped.
 *
 * If file is NULL an anon mmobj will be used to create a mapping
 * of 0's.  If file is non-null that vnode's file will be mapped in
 * for the given range.  Use the vnode's mmap operation to get the
 * mmobj for the file; do not assume it is file->vn_obj. Make sure all
 * of the area's fields except for vma_obj have been set before
 * calling mmap.
 *
 * If MAP_PRIVATE is specified set up a shadow object for the mmobj.
 *
 * All of the input to this function should be valid (KASSERT!).
 * See mmap(2) for for description of legal input.
 * Note that off should be page aligned.
 *
 * Be very careful about the order operations are performed in here. Some
 * operation are impossible to undo and should be saved until there
 * is no chance of failure.
 *
 * If 'new' is non-NULL a pointer to the new vmarea_t should be stored in it.
 */
int
vmmap_map(vmmap_t *map, vnode_t *file, uint32_t lopage, uint32_t npages,
          int prot, int flags, off_t off, int dir, vmarea_t **new)
{
        // NOT_YET_IMPLEMENTED("VM: vmmap_map");

        // must not add a memory segment into a non-existing vmmap
        KASSERT(NULL != map);
        // number of pages of this memory segment cannot be 0
        KASSERT(0 < npages);
        // must specify whether the memory segment is shared or private
        KASSERT((MAP_SHARED & flags) || (MAP_PRIVATE & flags));
        // if lopage is not zero, it must be a user space vpn
        KASSERT((0 == lopage) || (ADDR_TO_PN(USER_MEM_LOW) <= lopage));
        // if lopage is not zero, the specified page range must lie completely within the user space
        KASSERT((0 == lopage) || (ADDR_TO_PN(USER_MEM_HIGH) >= (lopage + npages)));
        // the off argument must be page aligned
        KASSERT(PAGE_ALIGNED(off));

        vmarea_t *vma;
        mmobj_t *o;
        int err;
        int range = 0;

        if(lopage == 0){
                // find a range of virtual addresses in the process that is big enough
                if((range = vmmap_find_range(map, npages, dir)) < 0){
                        return -1;
                }
                lopage = range;
        }
        else if(!vmmap_is_range_empty(map, lopage, npages)){
                /* seems no error will occur:
                 * we should not care about error in mmap, since to call it, we should
                 * make sure everything is set up except vma_obj, and to insert the vma,
                 * we should first remove the mapped region*/ 
                vmmap_remove(map, lopage, npages);
        }

        /* in both cases, create a new vma, set up the area's fields except vma_obj, 
         * and add to the address space */ 
        vma = vmarea_alloc();
        vma->vma_start = lopage;
        vma->vma_end = lopage + npages;
        vma->vma_flags = flags;
        // off should be page aligned.
        vma->vma_off = ADDR_TO_PN(off);
        vma->vma_prot = prot;
        list_init(&vma->vma_plink);
        list_init(&vma->vma_olink);
        vmmap_insert(map, vma);

        if(file == NULL){
                // an anon mmobj will be used to create a mapping of 0's
                o = anon_create();
        }
        else if((err = file->vn_ops->mmap(file, vma, &o)) < 0){
                // use the vnode's mmap operation to get the mmobj for the file
                // this will increase the refcount of file
                vmarea_free(vma);
                return err;
        }

        // in both cases, set mmobj's list and vma's vma_obj
        list_insert_tail(&o->mmo_un.mmo_vmas, &vma->vma_olink);
        vma->vma_obj = o;

        // if MAP_PRIVATE is specified set up a shadow object for the mmobj
        if(flags & MAP_PRIVATE){
                mmobj_t *bottom_obj;
                mmobj_t *shadow_obj = shadow_create();
                shadow_obj->mmo_shadowed = o;
                bottom_obj = mmobj_bottom_obj(o);
                shadow_obj->mmo_un.mmo_bottom_obj = bottom_obj;
                /* increment refcount of the bottom_obj, don't need to change the
                 * mmobj's refcount since the vma no longer refers to it*/  
                bottom_obj->mmo_ops->ref(bottom_obj);
                vma->vma_obj = shadow_obj;
        }

        // if 'new' is non-NULL a pointer to the new vmarea_t should be stored in it
        if(new != NULL){
                *new = vma;
        }

        return 0;
}

/*
 * We have no guarantee that the region of the address space being
 * unmapped will play nicely with our list of vmareas.
 *
 * You must iterate over each vmarea that is partially or wholly covered
 * by the address range [addr ... addr+len). The vm-area will fall into one
 * of four cases, as illustrated below:
 *
 * key:
 *          [             ]   Existing VM Area
 *        *******             Region to be unmapped
 *
 * Case 1:  [   ******    ]
 * The region to be unmapped lies completely inside the vmarea. We need to
 * split the old vmarea into two vmareas. be sure to increment the
 * reference count to the file associated with the vmarea.
 *
 * Case 2:  [      *******]**
 * The region overlaps the end of the vmarea. Just shorten the length of
 * the mapping.
 *
 * Case 3: *[*****        ]
 * The region overlaps the beginning of the vmarea. Move the beginning of
 * the mapping (remember to update vma_off), and shorten its length.
 *
 * Case 4: *[*************]**
 * The region completely contains the vmarea. Remove the vmarea from the
 * list.
 */
int
vmmap_remove(vmmap_t *map, uint32_t lopage, uint32_t npages)
{
        // NOT_YET_IMPLEMENTED("VM: vmmap_remove");

        vmarea_t *vma;
        uint32_t endvfn = lopage + npages;

        list_iterate_begin(&map->vmm_list, vma, vmarea_t, vma_plink)
        {
                // case 1: [   ******    ]
                if(vma->vma_end > endvfn && vma->vma_start < lopage){
                        vmarea_t *newvma = vmarea_alloc();
                        newvma->vma_end = vma->vma_end;
                        newvma->vma_start = endvfn;
                        newvma->vma_flags = vma->vma_flags;
                        newvma->vma_off = vma->vma_off + endvfn - vma->vma_start;
                        newvma->vma_prot = vma->vma_prot;
                        newvma->vma_obj = vma->vma_obj;
                        list_init(&newvma->vma_plink);
                        list_init(&newvma->vma_olink);
                        vmmap_insert(map,newvma);

                        /* be sure to increment the reference count to the 
                         * file associated with the vmarea, and change the vma_end
                         * of the old vma*/
                        newvma->vma_obj->mmo_ops->ref(newvma->vma_obj);
                        vma->vma_end = lopage;
                }

                // case 2: [      *******]**
                else if(vma->vma_end <= endvfn && vma->vma_start < lopage && vma->vma_end > lopage){
                        vma->vma_end = lopage;
                }

                // case 3: *[*****        ]
                else if(vma->vma_end > endvfn && vma->vma_start >= lopage && vma->vma_start < endvfn){
                        //remember to update vma_off
                        vma->vma_off += endvfn - vma->vma_start;
                        vma->vma_start = endvfn;
                }

                // case 4: *[*************]**
                else if(vma->vma_end <= endvfn && vma->vma_start >= lopage){
                        // remove the links
                        list_remove(&vma->vma_plink);
                        list_remove(&vma->vma_olink);

                        // don't forget to put the file
                        vma->vma_obj->mmo_ops->put(vma->vma_obj);

                        // free the vmarea
                        vmarea_free(vma);
                }
        }
        list_iterate_end();

        // after modifying, flush the TLB entry and unmap it from pt
        tlb_flush_all();
        pt_unmap_range(curproc->p_pagedir, (uintptr_t) PN_TO_ADDR(lopage), (uintptr_t) PN_TO_ADDR(lopage + npages));

        return 0;
}

/*
 * Returns 1 if the given address space has no mappings for the
 * given range, 0 otherwise.
 */
int
vmmap_is_range_empty(vmmap_t *map, uint32_t startvfn, uint32_t npages)
{
        // NOT_YET_IMPLEMENTED("VM: vmmap_is_range_empty");

        uint32_t endvfn = startvfn + npages;

        // the specified page range must not be empty and lie completely within the user space
        KASSERT((startvfn < endvfn) && (ADDR_TO_PN(USER_MEM_LOW) <= startvfn) && (ADDR_TO_PN(USER_MEM_HIGH) >= endvfn));

        vmarea_t *vma;
        list_iterate_begin(&map->vmm_list, vma, vmarea_t, vma_plink)
        {
                if(vma->vma_start >= endvfn){
                        // all the start addr of vmareas after this would > endvfn
                        return 1;
                }
                if(vma->vma_start < endvfn && vma->vma_end > startvfn){
                        return 0;
                }
        }
        list_iterate_end();

        return 1;
}

/* Read into 'buf' from the virtual address space of 'map' starting at
 * 'vaddr' for size 'count'. To do so, you will want to find the vmareas
 * to read from, then find the pframes within those vmareas corresponding
 * to the virtual addresses you want to read, and then read from the
 * physical memory that pframe points to. You should not check permissions
 * of the areas. Assume (KASSERT) that all the areas you are accessing exist.
 * Returns 0 on success, -errno on error.
 */
int
vmmap_read(vmmap_t *map, const void *vaddr, void *buf, size_t count)
{
        // NOT_YET_IMPLEMENTED("VM: vmmap_read");

        uint32_t readBytes = 0;
        uint32_t endaddr = (uint32_t)vaddr + count;
        uint32_t curraddr = (uint32_t)vaddr;

        while(curraddr < endaddr){
                // find the vmareas to read from, remember to change addr to PN
                vmarea_t *vma =  vmmap_lookup(map, ADDR_TO_PN(curraddr));

                // if no such vma is found, set the errno and return
                if(vma == NULL){
                        curthr->kt_errno = 1;
                        return -1;
                } 

                // find the pframes within those vmareas you want to read
                mmobj_t *o = vma->vma_obj;
                // current pagenum
                uint32_t pagenum = vma->vma_off + ADDR_TO_PN(curraddr) - vma->vma_start;
                pframe_t *pf;
                // lookup the page of current pagenum
                o->mmo_ops->lookuppage(o, pagenum, 0, &pf);
                // calculate the number of bytes to read
                uint32_t bytes = MIN(count - readBytes, PAGE_SIZE - PAGE_OFFSET(curraddr));

                // copy the content into buf
                memcpy((char*) buf + readBytes, (char*) pf->pf_addr + PAGE_OFFSET(curraddr), bytes);

                // increment curraddr and total read bytes
                curraddr += bytes;
                readBytes += bytes;
        }

        return 0;
}

/* Write from 'buf' into the virtual address space of 'map' starting at
 * 'vaddr' for size 'count'. To do this, you will need to find the correct
 * vmareas to write into, then find the correct pframes within those vmareas,
 * and finally write into the physical addresses that those pframes correspond
 * to. You should not check permissions of the areas you use. Assume (KASSERT)
 * that all the areas you are accessing exist. Remember to dirty pages!
 * Returns 0 on success, -errno on error.
 */
int
vmmap_write(vmmap_t *map, void *vaddr, const void *buf, size_t count)
{
        // NOT_YET_IMPLEMENTED("VM: vmmap_write");

        uint32_t writeBytes = 0;
        uint32_t endaddr = (uint32_t)vaddr + count;
        uint32_t curraddr = (uint32_t)vaddr; 

        while(curraddr < endaddr){
                // find the vmareas to write to, remember to change addr to PN
                vmarea_t *vma =  vmmap_lookup(map, ADDR_TO_PN(curraddr));

                // if no such vma is found, set the errno and return
                if(vma == NULL){
                        curthr->kt_errno = 1;
                        return -1;
                } 

                /* find the pframes within those vmareas you want to write */
                mmobj_t *o = vma->vma_obj;

                // current pagenum
                uint32_t pagenum = vma->vma_off + ADDR_TO_PN(curraddr) - vma->vma_start;
                pframe_t *pf;
                // lookup the page of current pagenum
                o->mmo_ops->lookuppage(o, pagenum, 1, &pf);
                // calculate the number of bytes to write
                uint32_t bytes = MIN(count - writeBytes, PAGE_SIZE - PAGE_OFFSET(curraddr));

                // copy the buf into page content
                memcpy((char*) pf->pf_addr + PAGE_OFFSET(curraddr), (char*) buf + writeBytes, bytes);

                // don't forget to dirty the page
                pframe_dirty(pf);

                // increment curraddr and total written bytes
                curraddr += bytes;
                writeBytes += bytes;
        }

        return 0;
}
