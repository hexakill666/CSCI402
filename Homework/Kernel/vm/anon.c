
#include "globals.h"
#include "errno.h"

#include "util/string.h"
#include "util/debug.h"

#include "mm/mmobj.h"
#include "mm/pframe.h"
#include "mm/mm.h"
#include "mm/page.h"
#include "mm/slab.h"
#include "mm/tlb.h"

int anon_count = 0; /* for debugging/verification purposes */

static slab_allocator_t *anon_allocator;

static void anon_ref(mmobj_t *o);
static void anon_put(mmobj_t *o);
static int  anon_lookuppage(mmobj_t *o, uint32_t pagenum, int forwrite, pframe_t **pf);
static int  anon_fillpage(mmobj_t *o, pframe_t *pf);
static int  anon_dirtypage(mmobj_t *o, pframe_t *pf);
static int  anon_cleanpage(mmobj_t *o, pframe_t *pf);

static mmobj_ops_t anon_mmobj_ops = {
        .ref = anon_ref,
        .put = anon_put,
        .lookuppage = anon_lookuppage,
        .fillpage  = anon_fillpage,
        .dirtypage = anon_dirtypage,
        .cleanpage = anon_cleanpage
};

/*
 * This function is called at boot time to initialize the
 * anonymous page sub system. Currently it only initializes the
 * anon_allocator object.
 */
void
anon_init()
{
        // NOT_YET_IMPLEMENTED("VM: anon_init");

        // currently it only initializes the anon_allocator object
        anon_allocator = slab_allocator_create("anon", sizeof(mmobj_t));

        // after initialization, anon_allocator must not be NULL
        KASSERT(anon_allocator);
}

/*
 * You'll want to use the anon_allocator to allocate the mmobj to
 * return, then initialize it. Take a look in mm/mmobj.h for
 * definitions which can be of use here. Make sure your initial
 * reference count is correct.
 */
mmobj_t *
anon_create()
{
        // NOT_YET_IMPLEMENTED("VM: anon_create");

        // allocate newMmObj memory
        mmobj_t* newMmObj = (mmobj_t*) slab_obj_alloc(anon_allocator);

        // fill in newMmObj attribute
        mmobj_init(newMmObj, &anon_mmobj_ops);
        newMmObj->mmo_refcount = 1;

        return newMmObj;
}

/* Implementation of mmobj entry points: */

/*
 * Increment the reference count on the object.
 */
static void
anon_ref(mmobj_t *o)
{
        // NOT_YET_IMPLEMENTED("VM: anon_ref");

        // the o function argument must be non-NULL, has a positive refcount, and is an anonymous object
        KASSERT(o && (0 < o->mmo_refcount) && (&anon_mmobj_ops == o->mmo_ops));

        o->mmo_refcount++;
}

/*
 * Decrement the reference count on the object. If, however, the
 * reference count on the object reaches the number of resident
 * pages of the object, we can conclude that the object is no
 * longer in use and, since it is an anonymous object, it will
 * never be used again. You should unpin and uncache all of the
 * object's pages and then free the object itself.
 */
static void
anon_put(mmobj_t *o)
{
        // NOT_YET_IMPLEMENTED("VM: anon_put");

        // the o function argument must be non-NULL, has a positive refcount, and is an anonymous object
        KASSERT(o && (0 < o->mmo_refcount) && (&anon_mmobj_ops == o->mmo_ops));

        // if, however, the reference count on the object reaches the number of 
        // resident pages of the object
        if(o->mmo_refcount - 1 == o->mmo_nrespages){
                // you should unpin and uncache all of the object's pages 
                // and then free the object itself
                pframe_t* curPframe = NULL;
                list_iterate_begin(&o->mmo_respages, curPframe, pframe_t, pf_olink)
                {
                        // unpin curPframe
                        if(pframe_is_pinned(curPframe)){
                                pframe_unpin(curPframe);
                        }
                        // uncache curPframe
                        pframe_free(curPframe);
                }
                list_iterate_end();
                // and then free the object itself
                o->mmo_refcount--;
                slab_obj_free(anon_allocator, o);
        }
        // decrease mmo_refcount
        else{
                o->mmo_refcount--;
        }
}

/* Get the corresponding page from the mmobj. No special handling is
 * required. */
static int
anon_lookuppage(mmobj_t *o, uint32_t pagenum, int forwrite, pframe_t **pf)
{
        // NOT_YET_IMPLEMENTED("VM: anon_lookuppage");

        int code = pframe_get(o, pagenum, pf);

        return code;
}

/* The following three functions should not be difficult. */

static int
anon_fillpage(mmobj_t *o, pframe_t *pf)
{
        // NOT_YET_IMPLEMENTED("VM: anon_fillpage");

        // can only "fill" a page frame when the page frame is in the "busy" state
        KASSERT(pframe_is_busy(pf));
        // must not fill a page frame that's already pinned
        KASSERT(!pframe_is_pinned(pf));
        
        // pin pframe
        pframe_pin(pf);

        // reset memory
        memset(pf->pf_addr, 0, PAGE_SIZE);

        return 0;
}

static int
anon_dirtypage(mmobj_t *o, pframe_t *pf)
{
        // NOT_YET_IMPLEMENTED("VM: anon_dirtypage");

        pframe_set_dirty(pf);

        return 0;
}

/**
 * Write the contents of the page frame starting at address pf->pf_paddr
 * to the page identified by pf->pf_obj and pf->pf_pagenum.
 *                                                         ----mmobj.h
*/
static int
anon_cleanpage(mmobj_t *o, pframe_t *pf)
{
        // NOT_YET_IMPLEMENTED("VM: anon_cleanpage");

        pframe_t* copyPframe;
        o->mmo_ops->lookuppage(o, pf->pf_pagenum, 1, &copyPframe);

        memcpy(copyPframe->pf_addr, pf->pf_addr, PAGE_SIZE);

        pframe_clear_dirty(copyPframe);

        return 0;
}
