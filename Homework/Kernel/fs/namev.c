
#include "kernel.h"
#include "globals.h"
#include "types.h"
#include "errno.h"

#include "util/string.h"
#include "util/printf.h"
#include "util/debug.h"

#include "fs/dirent.h"
#include "fs/fcntl.h"
#include "fs/stat.h"
#include "fs/vfs.h"
#include "fs/vnode.h"

/* This takes a base 'dir', a 'name', its 'len', and a result vnode.
 * Most of the work should be done by the vnode's implementation
 * specific lookup() function.
 *
 * If dir has no lookup(), return -ENOTDIR.
 *
 * Note: returns with the vnode refcount on *result incremented.
 */
int
lookup(vnode_t *dir, const char *name, size_t len, vnode_t **result)
{
        // NOT_YET_IMPLEMENTED("VFS: lookup");

        // the "dir" argument must be non-NULL
        KASSERT(NULL != dir);
        // the "name" argument must be non-NULL
        KASSERT(NULL != name);
        // the "result" argument must be non-NULL
        KASSERT(NULL != result);

        // if dir has no lookup()
        if(NULL == dir->vn_ops->lookup){
                return -ENOTDIR;
        }
        // if dir has lookup(), let it do the work
        int code = dir->vn_ops->lookup(dir, name, len, result);

        return code;
}


/* When successful this function returns data in the following "out"-arguments:
 *  o res_vnode: the vnode of the parent directory of "name"
 *  o name: the `basename' (the element of the pathname)
 *  o namelen: the length of the basename
 *
 * For example: dir_namev("/s5fs/bin/ls", &namelen, &name, NULL,
 * &res_vnode) would put 2 in namelen, "ls" in name, and a pointer to the
 * vnode corresponding to "/s5fs/bin" in res_vnode.
 *
 * The "base" argument defines where we start resolving the path from:
 * A base value of NULL means to use the process's current working directory,
 * curproc->p_cwd.  If pathname[0] == '/', ignore base and start with
 * vfs_root_vn.  dir_namev() should call lookup() to take care of resolving each
 * piece of the pathname.
 *
 * Note: A successful call to this causes vnode refcount on *res_vnode to
 * be incremented.
 */
int
dir_namev(const char *pathname, size_t *namelen, const char **name,
          vnode_t *base, vnode_t **res_vnode)
{
        // NOT_YET_IMPLEMENTED("VFS: dir_namev");

        // the "pathname" argument must be non-NULL
        KASSERT(NULL != pathname);
        // the "namelen" argument must be non-NULL
        KASSERT(NULL != namelen);
        // the "name" argument must be non-NULL
        KASSERT(NULL != name);
        // the "res_vnode" argument must be non-NULL
        KASSERT(NULL != res_vnode);

        // change curBase according to different cases --check pathname first
        vnode_t* curBase = base;
        if('/' == pathname[0]){
                curBase = vfs_root_vn;
        }
        else if(NULL == base){
                curBase = curproc->p_cwd;
        }
        // pathname resolution must start with a valid directory
        KASSERT(NULL != curBase);

        // find index limit of res_vnode parent directory
        // exclude last file of pathname
        int resVNodeDirIndexLimit = strlen(pathname) - 1;
        int last_slash_len = 0;
        while(resVNodeDirIndexLimit >= 0 && pathname[resVNodeDirIndexLimit] == '/'){
                resVNodeDirIndexLimit--;
                last_slash_len++;
        }
        while(resVNodeDirIndexLimit >= 0){
                if('/' == pathname[resVNodeDirIndexLimit]){
                        break;
                }
                resVNodeDirIndexLimit--;
        }
        // set last file attribute
        *namelen = strlen(pathname) - last_slash_len - (resVNodeDirIndexLimit + 1);
        // if pathname has last file
        if(*namelen > 0){
                // check last file name length
                if(*namelen > NAME_LEN){
                        return -ENAMETOOLONG;
                }
                // if pathname has at least 1 slash
                if(resVNodeDirIndexLimit >= 0){
                        *name = &pathname[resVNodeDirIndexLimit] + 1;
                }
                // if pathname has no slash, pathname only has 1 relative path
                else{
                        *name = &pathname[0];
                }
        }
        // if pathname has no last file
        else{
                *name = NULL;
        }
        
        int slow = 0;
        int fast = 0;
        vnode_t* curVNode = NULL;
        vnode_t *prevVNode = NULL;

        // skip slash
        while(fast < resVNodeDirIndexLimit){
                if('/' != pathname[fast]){
                        break;
                }
                fast++;
        }

        // try to search each file in pathname until index limit
        while(fast < resVNodeDirIndexLimit){
                // first non-slash char
                slow = fast;
                // search the whole file name until slash or end
                while(fast < resVNodeDirIndexLimit){
                        if('/' == pathname[fast]){
                                break;
                        }
                        fast++;
                }

                // found next file at pathname[slow, fast)
                // check next file name length
                if(fast - slow > NAME_LEN){
                        vput(curVNode);
                        return -ENAMETOOLONG;
                }
                // lookup next file from curBase
                int code = lookup(curBase, &pathname[slow], fast - slow, &curVNode);

                // we increase VNode vn_refcount(due to successful lookup()) of each file in pathname
                // so we decrease VNode vn_refcount of each file in pathname
                // we only want to increase VNode vn_refcount of last file in pathname
                if(prevVNode != NULL){
                        vput(prevVNode);
                }
                // if code < 0, next file does not exist
                if(code < 0){
                        return code;
                }

                // change curBase to next file, prevVNode to curVNode
                prevVNode = curVNode;
                curBase = curVNode;

                // skip slash
                while(fast < resVNodeDirIndexLimit){
                        if('/' != pathname[fast]){
                                break;
                        }
                        fast++;
                }
        }

        // if pathname has >= 2 directory depth(i.e. /../. or /../)
        if(NULL != curVNode){
                if(curBase->vn_ops->lookup == NULL){
                        vput(curVNode);
                        return -ENOTDIR;
                }
                // return parent directory VNode of last file
                *res_vnode = curVNode;
        }
        // if pathname has < 2 directory depth(i.e. /.. or /)
        else{
                // return curBase VNode
                *res_vnode = curBase;
                vref(*res_vnode);
        }

        return 0;
}

/* This returns in res_vnode the vnode requested by the other parameters.
 * It makes use of dir_namev and lookup to find the specified vnode (if it
 * exists).  flag is right out of the parameters to open(2); see
 * <weenix/fcntl.h>.  If the O_CREAT flag is specified and the file does
 * not exist, call create() in the parent directory vnode. However, if the
 * parent directory itself does not exist, this function should fail - in all
 * cases, no files or directories other than the one at the very end of the path
 * should be created.
 *
 * Note: Increments vnode refcount on *res_vnode.
 */
int
open_namev(const char *pathname, int flag, vnode_t **res_vnode, vnode_t *base)
{
        // NOT_YET_IMPLEMENTED("VFS: open_namev");

        size_t namelen = 0;
        const char* name = NULL;
        vnode_t* resVNodeDir = NULL;

        // resolve pathname
        int code = dir_namev(pathname, &namelen, &name, base, &resVNodeDir);
        // if code < 0, parent directory does not exist
        if(code < 0){
                return code;
        }

        // if namelen == 0, pathname has no last file
        // but pathname exists, so there is no need to create any file
        if(namelen == 0){
                // set res_vnode to parent directory of res_vnode
                *res_vnode = resVNodeDir;
                return 0;
        }
        // if pathname has last file
        // lookup last file of pathname
        code = lookup(resVNodeDir, name, namelen, res_vnode);

        // if code < 0, last file of pathname does not exist
        if(code < 0){
                // if O_CREAT flag is specified
                if(flag & O_CREAT){
                        // it is time to create last file
                        // check create() of res_vnode parent directory
                        KASSERT(NULL != resVNodeDir->vn_ops->create);

                        // create last file
                        code = resVNodeDir->vn_ops->create(resVNodeDir, name, namelen, res_vnode);
                        // create() has set res_vnode VNode and increased res_vnode vn_refcount
                        vput(resVNodeDir);
                        
                        return code;
                }
                // if O_CREAT flag is not specified
                vput(resVNodeDir);

                return code;
        }
        // if code == 0, last file of pathname exists

        // check if pathname requires a directory
        if(pathname[strlen(pathname) - 1] == '/' && !S_ISDIR((*res_vnode)->vn_mode)){
                vput((*res_vnode));
                vput(resVNodeDir);
                return -ENOTDIR;
        }
        // lookup() has set res_vnode VNode and increased res_vnode vn_refcount
        vput(resVNodeDir);

        return 0;
}

#ifdef __GETCWD__
/* Finds the name of 'entry' in the directory 'dir'. The name is writen
 * to the given buffer. On success 0 is returned. If 'dir' does not
 * contain 'entry' then -ENOENT is returned. If the given buffer cannot
 * hold the result then it is filled with as many characters as possible
 * and a null terminator, -ERANGE is returned.
 *
 * Files can be uniquely identified within a file system by their
 * inode numbers. */
int
lookup_name(vnode_t *dir, vnode_t *entry, char *buf, size_t size)
{
        NOT_YET_IMPLEMENTED("GETCWD: lookup_name");
        return -ENOENT;
}


/* Used to find the absolute path of the directory 'dir'. Since
 * directories cannot have more than one link there is always
 * a unique solution. The path is writen to the given buffer.
 * On success 0 is returned. On error this function returns a
 * negative error code. See the man page for getcwd(3) for
 * possible errors. Even if an error code is returned the buffer
 * will be filled with a valid string which has some partial
 * information about the wanted path. */
ssize_t
lookup_dirpath(vnode_t *dir, char *buf, size_t osize)
{
        NOT_YET_IMPLEMENTED("GETCWD: lookup_dirpath");

        return -ENOENT;
}
#endif /* __GETCWD__ */
