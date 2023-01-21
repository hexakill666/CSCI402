
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

#include "vm/vmmap.h"
#include "vm/shadow.h"
#include "vm/shadowd.h"

#define SHADOW_SINGLETON_THRESHOLD 5

int shadow_count = 0; /* for debugging/verification purposes */
#ifdef __SHADOWD__
/*
 * number of shadow objects with a single parent, that is another shadow
 * object in the shadow objects tree(singletons)
 */
static int shadow_singleton_count = 0;
#endif

static slab_allocator_t *shadow_allocator;

static void shadow_ref(mmobj_t *o);
static void shadow_put(mmobj_t *o);
static int  shadow_lookuppage(mmobj_t *o, uint32_t pagenum, int forwrite, pframe_t **pf);
static int  shadow_fillpage(mmobj_t *o, pframe_t *pf);
static int  shadow_dirtypage(mmobj_t *o, pframe_t *pf);
static int  shadow_cleanpage(mmobj_t *o, pframe_t *pf);

static mmobj_ops_t shadow_mmobj_ops = {
        .ref = shadow_ref,
        .put = shadow_put,
        .lookuppage = shadow_lookuppage,
        .fillpage  = shadow_fillpage,
        .dirtypage = shadow_dirtypage,
        .cleanpage = shadow_cleanpage
};

/*
 * This function is called at boot time to initialize the
 * shadow page sub system. Currently it only initializes the
 * shadow_allocator object.
 */
void
shadow_init()
{
        // NOT_YET_IMPLEMENTED("VM: shadow_init");

        // currently it only initializes the shadow_allocator object.
        shadow_allocator = slab_allocator_create("shadow", sizeof(mmobj_t));

        // after initialization, shadow_allocator must not be NULL
        KASSERT(shadow_allocator);
}

/*
 * You'll want to use the shadow_allocator to allocate the mmobj to
 * return, then then initialize it. Take a look in mm/mmobj.h for
 * macros or functions which can be of use here. Make sure your initial
 * reference count is correct.
 */
mmobj_t *
shadow_create()
{
        // NOT_YET_IMPLEMENTED("VM: shadow_create");

        // allocate newMmObj memory
        mmobj_t* newMmObj = (mmobj_t*) slab_obj_alloc(shadow_allocator);

        // fill in newMmObj attribute
        mmobj_init(newMmObj, &shadow_mmobj_ops);
        newMmObj->mmo_refcount = 1;

        return newMmObj;
}

/* Implementation of mmobj entry points: */

/*
 * Increment the reference count on the object.
 */
static void
shadow_ref(mmobj_t *o)
{
        // NOT_YET_IMPLEMENTED("VM: shadow_ref");

        // the o function argument must be non-NULL, has a positive refcount, and is a shadow object
        KASSERT(o && (0 < o->mmo_refcount) && (&shadow_mmobj_ops == o->mmo_ops));

        o->mmo_refcount++;
}

/*
 * Decrement the reference count on the object. If, however, the
 * reference count on the object reaches the number of resident
 * pages of the object, we can conclude that the object is no
 * longer in use and, since it is a shadow object, it will never
 * be used again. You should unpin and uncache all of the object's
 * pages and then free the object itself.
 */
static void
shadow_put(mmobj_t *o)
{
        // NOT_YET_IMPLEMENTED("VM: shadow_put");

        // the o function argument must be non-NULL, has a positive refcount, and is a shadow object
        KASSERT(o && (0 < o->mmo_refcount) && (&shadow_mmobj_ops == o->mmo_ops));

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
                // remember to put the shadowed and bottom mmobj
                o->mmo_un.mmo_bottom_obj->mmo_ops->put(o->mmo_un.mmo_bottom_obj);
                o->mmo_shadowed->mmo_ops->put(o->mmo_shadowed);
                // and then free the object itself
                o->mmo_refcount--;
                slab_obj_free(shadow_allocator, o);
        }
        // decrease mmo_refcount
        else{
                o->mmo_refcount--;
        }
}

/* This function looks up the given page in this shadow object. The
 * forwrite argument is true if the page is being looked up for
 * writing, false if it is being looked up for reading. This function
 * must handle all do-not-copy-on-not-write magic (i.e. when forwrite
 * is false find the first shadow object in the chain which has the
 * given page resident). copy-on-write magic (necessary when forwrite
 * is true) is handled in shadow_fillpage, not here. It is important to
 * use iteration rather than recursion here as a recursive implementation
 * can overflow the kernel stack when looking down a long shadow chain */
static int
shadow_lookuppage(mmobj_t *o, uint32_t pagenum, int forwrite, pframe_t **pf)
{
        // NOT_YET_IMPLEMENTED("VM: shadow_lookuppage");

        // if forwrite is true, copy-on-write magic
        if(forwrite){
                // only need to find the corresponding pframe to deal with copy-on-write
                pframe_t* cowPframe = pframe_get_resident(o, pagenum);

                // if cowPframe == NULL, cowPframe does not exist
                if(NULL == cowPframe){
                        // create a new pframe
                        int code = pframe_get(o, pagenum, pf);
                        // if code < 0, pframe_get fails
                        if(code < 0){
                                return code;
                        }
                        // otherwise, pframe_get succeeds
                        // because forwrite is true, dirty the page
                        pframe_dirty(*pf);
                }
                // if cowPframe != NULL, cowPframe exists
                else{
                        *pf = cowPframe;
                }
        }
        // if forwrite is false, do-not-copy-on-not-write magic
        else{
                // find the first shadow object in the chain which has the given page resident
                mmobj_t* curMmObj = o;
                pframe_t* shadowPframe = NULL;

                while(curMmObj->mmo_shadowed != NULL){
                        // search shadowPframe
                        shadowPframe = pframe_get_resident(curMmObj, pagenum);
                        // found shadowPframe
                        if(NULL != shadowPframe){
                                *pf = shadowPframe;
                                break;
                        }
                        // next shadow mmobj
                        curMmObj = curMmObj->mmo_shadowed;
                }

                // if shadowPframe == NULL, still cannot find one
                if(NULL == shadowPframe){
                        // then check the bottom mmobj
                        int code = pframe_lookup(o->mmo_un.mmo_bottom_obj, pagenum, 0, pf);
                        // if code < 0, pframe_lookup fails
                        if(code < 0){
                                return code;
                        }
                }
        }

        // on return, (*pf) must be non-NULL
        KASSERT(NULL != (*pf));
        // on return, the page frame must have the right pagenum and it must not be in the "busy" state
        KASSERT((pagenum == (*pf)->pf_pagenum) && (!pframe_is_busy(*pf)));

        return 0;
}

/* As per the specification in mmobj.h, fill the page frame starting
 * at address pf->pf_addr with the contents of the page identified by
 * pf->pf_obj and pf->pf_pagenum. This function handles all
 * copy-on-write magic (i.e. if there is a shadow object which has
 * data for the pf->pf_pagenum-th page then we should take that data,
 * if no such shadow object exists we need to follow the chain of
 * shadow objects all the way to the bottom object and take the data
 * for the pf->pf_pagenum-th page from the last object in the chain).
 * It is important to use iteration rather than recursion here as a
 * recursive implementation can overflow the kernel stack when
 * looking down a long shadow chain */
static int
shadow_fillpage(mmobj_t *o, pframe_t *pf)
{
        // NOT_YET_IMPLEMENTED("VM: shadow_fillpage");

        // can only "fill" a page frame when the page frame is in the "busy" state
        KASSERT(pframe_is_busy(pf));
        // must not fill a page frame that's already pinned
        KASSERT(!pframe_is_pinned(pf));

        mmobj_t* curMmObj = o;
        pframe_t* shadowPframe = NULL;

        // if curMmObj has shadow mmobj
        while(curMmObj->mmo_shadowed != NULL){
                // search shadowPframe
                shadowPframe = pframe_get_resident(curMmObj->mmo_shadowed, pf->pf_pagenum);
                // found shadowPframe
                if(NULL != shadowPframe){
                        // copy data
                        memcpy(pf->pf_addr, shadowPframe->pf_addr, PAGE_SIZE);
                        // pin pf
                        pframe_pin(pf);
                        break;
                }
                // next shadow mmobj
                curMmObj = curMmObj->mmo_shadowed;
        }

        // if shadowPframe == NULL, still cannot find one
        if(NULL == shadowPframe){
                // then check the bottom mmobj
                int code = pframe_lookup(o->mmo_un.mmo_bottom_obj, pf->pf_pagenum, 0, &shadowPframe);
                // if code < 0, pframe_lookup fails
                if(code < 0){
                        return code;
                }
                // otherwise, pframe_lookup succeeds
                // copy data
                memcpy(pf->pf_addr, shadowPframe->pf_addr, PAGE_SIZE);
                // pin pf
                pframe_pin(pf);
        }

        return 0;
}

/* These next two functions are not difficult. */

static int
shadow_dirtypage(mmobj_t *o, pframe_t *pf)
{
        // NOT_YET_IMPLEMENTED("VM: shadow_dirtypage");

        pframe_set_dirty(pf);

        return 0;
}

/**
 * Write the contents of the page frame starting at address pf->pf_paddr
 * to the page identified by pf->pf_obj and pf->pf_pagenum.
 *                                                         ----mmobj.h
*/
static int
shadow_cleanpage(mmobj_t *o, pframe_t *pf)
{
        // NOT_YET_IMPLEMENTED("VM: shadow_cleanpage");

        pframe_t* copyPframe;
        o->mmo_ops->lookuppage(o, pf->pf_pagenum, 1, &copyPframe);

        memcpy(copyPframe->pf_addr, pf->pf_addr, PAGE_SIZE);

        pframe_clear_dirty(copyPframe);

        return 0;
}
