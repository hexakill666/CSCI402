
/*
 *  FILE: open.c
 *  AUTH: mcc | jal
 *  DESC:
 *  DATE: Mon Apr  6 19:27:49 1998
 */

#include "globals.h"
#include "errno.h"
#include "fs/fcntl.h"
#include "util/string.h"
#include "util/printf.h"
#include "fs/vfs.h"
#include "fs/vnode.h"
#include "fs/file.h"
#include "fs/vfs_syscall.h"
#include "fs/open.h"
#include "fs/stat.h"
#include "util/debug.h"

/* find empty index in p->p_files[] */
int
get_empty_fd(proc_t *p)
{
        int fd;

        for (fd = 0; fd < NFILES; fd++) {
                if (!p->p_files[fd])
                        return fd;
        }

        dbg(DBG_ERROR | DBG_VFS, "ERROR: get_empty_fd: out of file descriptors "
            "for pid %d\n", curproc->p_pid);
        return -EMFILE;
}

/*
 * There a number of steps to opening a file:
 *      1. Get the next empty file descriptor.
 *      2. Call fget to get a fresh file_t.
 *      3. Save the file_t in curproc's file descriptor table.
 *      4. Set file_t->f_mode to OR of FMODE_(READ|WRITE|APPEND) based on
 *         oflags, which can be O_RDONLY, O_WRONLY or O_RDWR, possibly OR'd with
 *         O_APPEND.
 *      5. Use open_namev() to get the vnode for the file_t.
 *      6. Fill in the fields of the file_t.
 *      7. Return new fd.
 *
 * If anything goes wrong at any point (specifically if the call to open_namev
 * fails), be sure to remove the fd from curproc, fput the file_t and return an
 * error.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EINVAL
 *        oflags is not valid.
 *      o EMFILE
 *        The process already has the maximum number of files open.
 *      o ENOMEM
 *        Insufficient kernel memory was available.
 *      o ENAMETOOLONG
 *        A component of filename was too long.
 *      o ENOENT
 *        O_CREAT is not set and the named file does not exist.  Or, a
 *        directory component in pathname does not exist.
 *      o EISDIR
 *        pathname refers to a directory and the access requested involved
 *        writing (that is, O_WRONLY or O_RDWR is set).
 *      o ENXIO
 *        pathname refers to a device special file and no corresponding device
 *        exists.
 */

int
do_open(const char *filename, int oflags)
{
        // NOT_YET_IMPLEMENTED("VFS: do_open");

        /* No invalid combinations of O_RDONLY, O_WRONLY, and O_RDWR.  Since
         * O_RDONLY is stupidly defined as 0, the only invalid possible
         * combination is O_WRONLY|O_RDWR. 
         *                                              ----vfstest_open()
         * */
        // check oflags
        if((oflags & O_WRONLY) && (oflags & O_RDWR)){
                return -EINVAL;
        }

        // 1. Get the next empty file descriptor.
        int newfd = get_empty_fd(curproc);
        // if newfd < 0, get_empty_fd falis
        // case: -EMFILE
        if(newfd < 0){
                return newfd;
        }

        // 2. Call fget to get a fresh file_t.
        file_t* newFile = fget(-1);

        // 3. Save the file_t in curproc's file descriptor table.
        curproc->p_files[newfd] = newFile;

        //  4. Set file_t->f_mode to OR of FMODE_(READ|WRITE|APPEND) based on
        //  oflags, which can be O_RDONLY, O_WRONLY or O_RDWR, possibly OR'd with
        //  O_APPEND.
        newFile->f_mode = 0;
        
        if(oflags == O_RDONLY){
                newFile->f_mode |= FMODE_READ;
        }
        if(oflags & O_WRONLY){
                newFile->f_mode |= FMODE_WRITE;
        }
        if(oflags & O_RDWR){
                newFile->f_mode |= (FMODE_READ | FMODE_WRITE);
        }
        if(oflags & O_APPEND){
                newFile->f_mode |= FMODE_APPEND;
        }

        // 5. Use open_namev() to get the vnode for the file_t.
        vnode_t* resVNode = NULL;
        int code = open_namev(filename, oflags, &resVNode, NULL);
        // if code < 0, open_namev() fails
        // case: -ENOENT, -ENOTDIR, and -ENAMETOOLONG
        if(code < 0){
                // fput the newFile
                fput(newFile);
                // remove the newfd from curproc
                curproc->p_files[newfd] = NULL;
                return code;
        }

        // check -EISDIR
        if(S_ISDIR(resVNode->vn_mode) && ((oflags & O_WRONLY) || (oflags & O_RDWR))){
                // decrease resVNode vn_refcount
                vput(resVNode);
                // fput the newFile
                fput(newFile);
                // remove the newfd from curproc
                curproc->p_files[newfd] = NULL;
                return -EISDIR;
        }

        // even if resVNode is a device special file
        // there is no need to check -ENXIO
        // because code >= 0 means resVNode exists

        // 6. Fill in the fields of the file_t.
        newFile->f_pos = 0;
        newFile->f_vnode = resVNode;

        // 7. Return new fd.
        return newfd;
}
