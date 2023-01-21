
/*
 *  FILE: vfs_syscall.c
 *  AUTH: mcc | jal
 *  DESC:
 *  DATE: Wed Apr  8 02:46:19 1998
 *  $Id: vfs_syscall.c,v 1.2 2018/05/27 03:57:26 cvsps Exp $
 */

#include "kernel.h"
#include "errno.h"
#include "globals.h"
#include "fs/vfs.h"
#include "fs/file.h"
#include "fs/vnode.h"
#include "fs/vfs_syscall.h"
#include "fs/open.h"
#include "fs/fcntl.h"
#include "fs/lseek.h"
#include "mm/kmalloc.h"
#include "util/string.h"
#include "util/printf.h"
#include "fs/stat.h"
#include "util/debug.h"

/*
 * Syscalls for vfs. Refer to comments or man pages for implementation.
 * Do note that you don't need to set errno, you should just return the
 * negative error code.
 */

/* To read a file:
 *      o fget(fd)
 *      o call its virtual read vn_op
 *      o update f_pos
 *      o fput() it
 *      o return the number of bytes read, or an error
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd is not a valid file descriptor or is not open for reading.
 *      o EISDIR
 *        fd refers to a directory.
 *
 * In all cases, be sure you do not leak file refcounts by returning before
 * you fput() a file that you fget()'ed.
 */
int
do_read(int fd, void *buf, size_t nbytes)
{
        // NOT_YET_IMPLEMENTED("VFS: do_read");
        
        // check fd
        if(fd < 0 || fd >= NFILES || NULL == curproc->p_files[fd]){
                return -EBADF;
        }

        // 1. fget()
        file_t *file = fget(fd);
        // check if file is a directory
        if(file->f_vnode->vn_ops->read == NULL){
                fput(file);
                return -EISDIR;
        }
        // check if fd is not a valid file descriptor or is not open for reading
        if(!(file->f_mode & FMODE_READ)){
                fput(file);
                return -EBADF;
        }

        // 2. call virtual read
        int rbytes = file->f_vnode->vn_ops->read(file->f_vnode, file->f_pos, buf, nbytes);
        if(rbytes < 0){
                fput(file);
                return rbytes;
        }

        // 3. update f_ops
        file->f_pos += rbytes;

        // 4. fput()
        fput(file);

        return rbytes;
}

/* Very similar to do_read.  Check f_mode to be sure the file is writable.  If
 * f_mode & FMODE_APPEND, do_lseek() to the end of the file, call the write
 * vn_op, and fput the file.  As always, be mindful of refcount leaks.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd is not a valid file descriptor or is not open for writing.
 */
int
do_write(int fd, const void *buf, size_t nbytes)
{
        // NOT_YET_IMPLEMENTED("VFS: do_write");

        // check fd
        if(fd < 0 || fd >= NFILES || NULL == curproc->p_files[fd]){
                return -EBADF;
        }

        // 1. fget()
        file_t *file = fget(fd);
        //check if fd is not a valid file descriptor or is not open for reading
        if(!(file->f_mode & (FMODE_WRITE | FMODE_APPEND))){
                fput(file);
                return -EBADF;
        }

        // 2. call virtual write
        // check if the mode is FMODE_APPEND
        if(file->f_mode & FMODE_APPEND){
                do_lseek(fd, 0, SEEK_END);
        }
        int wbytes = file->f_vnode->vn_ops->write(file->f_vnode, file->f_pos, buf, nbytes);
        file->f_pos += wbytes;

        // cursor must not go past end of file for these file types
        KASSERT((S_ISCHR(file->f_vnode->vn_mode)) || (S_ISBLK(file->f_vnode->vn_mode)) ||((S_ISREG(file->f_vnode->vn_mode)) && (file->f_pos <= file->f_vnode->vn_len)));

        // 3. fput()
        fput(file);

        return wbytes;
}

/*
 * Zero curproc->p_files[fd], and fput() the file. Return 0 on success
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd isn't a valid open file descriptor.
 */
int
do_close(int fd)
{
        // NOT_YET_IMPLEMENTED("VFS: do_close");
        
        // check fd
        if(fd < 0 || fd >= NFILES || NULL == curproc->p_files[fd]){
                return -EBADF;
        }

        // fput() the file
        fput(curproc->p_files[fd]);

        // Zero curproc->p_files[fd]
        curproc->p_files[fd] = NULL;
        
        return 0;
}

/* To dup a file:
 *      o fget(fd) to up fd's refcount
 *      o get_empty_fd()
 *      o point the new fd to the same file_t* as the given fd
 *      o return the new file descriptor
 *
 * Don't fput() the fd unless something goes wrong.  Since we are creating
 * another reference to the file_t*, we want to up the refcount.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd isn't an open file descriptor.
 *      o EMFILE
 *        The process already has the maximum number of file descriptors open
 *        and tried to open a new one.
 */
int
do_dup(int fd)
{
        // NOT_YET_IMPLEMENTED("VFS: do_dup");

        // check fd
        if(fd < 0 || fd >= NFILES || NULL == curproc->p_files[fd]){
                return -EBADF;
        }

        // 1. fget(fd) to up fd's refcount
        file_t *file = fget(fd);

        // 2. get_empty_fd()
        int nfd = get_empty_fd(curproc);
        // check nfd
        if(nfd < 0){
                fput(file);
                return -EMFILE;
        }

        // 3. point the new fd to the same file_t* as the given fd
        curproc->p_files[nfd] = file;
        
        // 4. return the new file descriptor
        return nfd;
}

/* Same as do_dup, but insted of using get_empty_fd() to get the new fd,
 * they give it to us in 'nfd'.  If nfd is in use (and not the same as ofd)
 * do_close() it first.  Then return the new file descriptor.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        ofd isn't an open file descriptor, or nfd is out of the allowed
 *        range for file descriptors.
 */
int
do_dup2(int ofd, int nfd)
{
        // NOT_YET_IMPLEMENTED("VFS: do_dup2");

        // check ofd and nfd
        if(ofd < 0 || ofd >= NFILES || NULL == curproc->p_files[ofd] || nfd < 0 || nfd >= NFILES){
                return -EBADF;
        }

        file_t *file = fget(ofd);
        if(curproc->p_files[nfd] != NULL && nfd != ofd){
                do_close(nfd);
        }

        curproc->p_files[nfd] = file;
        if(nfd == ofd){
                fput(file);
        }
        
        return nfd;
}

/*
 * This routine creates a special file of the type specified by 'mode' at
 * the location specified by 'path'. 'mode' should be one of S_IFCHR or
 * S_IFBLK (you might note that mknod(2) normally allows one to create
 * regular files as well-- for simplicity this is not the case in Weenix).
 * 'devid', as you might expect, is the device identifier of the device
 * that the new special file should represent.
 *
 * You might use a combination of dir_namev, lookup, and the fs-specific
 * mknod (that is, the containing directory's 'mknod' vnode operation).
 * Return the result of the fs-specific mknod, or an error.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EINVAL
 *        mode requested creation of something other than a device special
 *        file.
 *      o EEXIST
 *        path already exists.
 *      o ENOENT
 *        A directory component in path does not exist.
 *      o ENOTDIR
 *        A component used as a directory in path is not, in fact, a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
int
do_mknod(const char *path, int mode, unsigned devid)
{
        // NOT_YET_IMPLEMENTED("VFS: do_mknod");

        // if mode is not S_IFCHR and S_IFBLK
        if(!S_ISCHR(mode) && !S_ISBLK(mode)){
                return -EINVAL;
        }

        size_t namelen = 0;
        const char* name = NULL;
        vnode_t* resVNodeDir = NULL;

        // resolve parent directory of last file
        int code = dir_namev(path, &namelen, &name, NULL, &resVNodeDir);
        // if code < 0, dir_namev() fails
        // case: -ENOENT, -ENOTDIR, and -ENAMETOOLONG
        if(code < 0){
                return code;
        }

        vnode_t* resVNode = NULL;
        // lookup last file
        code = lookup(resVNodeDir, name, namelen, &resVNode);
        // if code == 0, resVNode exists
        if(code == 0){
                // we have increased resVNode vn_refcount in lookup()
                // so we decrease resVNode vn_refcount
                vput(resVNode);
                vput(resVNodeDir);
                return -EEXIST;
        }

        // dir_vnode is the directory vnode where you will create the target special file
        KASSERT(NULL != resVNodeDir->vn_ops->mknod);
        // call mknod() of resVNodeDir
        code = resVNodeDir->vn_ops->mknod(resVNodeDir, name, namelen, mode, devid);
        vput(resVNodeDir);

        return code;
}

/* Use dir_namev() to find the vnode of the dir we want to make the new
 * directory in.  Then use lookup() to make sure it doesn't already exist.
 * Finally call the dir's mkdir vn_ops. Return what it returns.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EEXIST
 *        path already exists.
 *      o ENOENT
 *        A directory component in path does not exist.
 *      o ENOTDIR
 *        A component used as a directory in path is not, in fact, a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
int
do_mkdir(const char *path)
{
        // NOT_YET_IMPLEMENTED("VFS: do_mkdir");

        size_t namelen = 0;
        const char* name = NULL;
        vnode_t* resVNodeDir = NULL;

        // resolve parent directory of last file
        int code = dir_namev(path, &namelen, &name, NULL, &resVNodeDir);
        // if code < 0, dir_namev() fails
        // case: -ENOENT, -ENOTDIR, and -ENAMETOOLONG
        if(code < 0){
                return code;
        }

        vnode_t* resVNode = NULL;
        // if path is simply a combination of '/', return -EEXIST
        if(namelen == 0 && resVNodeDir == vfs_root_vn){
                vput(resVNodeDir);
                return -EEXIST;
        }

        // lookup last file
        code = lookup(resVNodeDir, name, namelen, &resVNode);
        // if code == 0, resVNode exists
        if(code == 0){
                // we have increased resVNode vn_refcount in lookup()
                // so we decrease resVNode vn_refcount
                vput(resVNode);
                vput(resVNodeDir);
                return -EEXIST;
        }

        // dir_vnode is the directory vnode where you will create the target directory
        KASSERT(NULL != resVNodeDir->vn_ops->mkdir);
        // call mkdir() of resVNodeDir
        code = resVNodeDir->vn_ops->mkdir(resVNodeDir, name, namelen);
        vput(resVNodeDir);

        return code;
}

/* Use dir_namev() to find the vnode of the directory containing the dir to be
 * removed. Then call the containing dir's rmdir v_op.  The rmdir v_op will
 * return an error if the dir to be removed does not exist or is not empty, so
 * you don't need to worry about that here. Return the value of the v_op,
 * or an error.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EINVAL
 *        path has "." as its final component.
 *      o ENOTEMPTY
 *        path has ".." as its final component.
 *      o ENOENT
 *        A directory component in path does not exist.
 *      o ENOTDIR
 *        A component used as a directory in path is not, in fact, a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
int
do_rmdir(const char *path)
{
        // NOT_YET_IMPLEMENTED("VFS: do_rmdir");

        size_t namelen = 0;
        const char* name = NULL;
        vnode_t* resVNodeDir = NULL;

        // resolve parent directory of last file
        int code = dir_namev(path, &namelen, &name, NULL, &resVNodeDir);
        // if code < 0, dir_namev() fails
        // case: -ENOENT, -ENOTDIR, and -ENAMETOOLONG
        if(code < 0){
                return code;
        }

        // check if last file is .
        if(name_match(name, ".", namelen)){
                vput(resVNodeDir);
                return -EINVAL;
        }
        // check if last file is ..
        if(name_match(name, "..", namelen)){
                vput(resVNodeDir);
                return -ENOTEMPTY;
        }

        // dir_vnode is the directory vnode where you will remove the target directory
        KASSERT(NULL != resVNodeDir->vn_ops->rmdir);
        // call rmdir() of resVNodeDir
        code = resVNodeDir->vn_ops->rmdir(resVNodeDir, name, namelen);
        // we have decreased VNode vn_refcount of last file in rmdir(), just decrement the dir refcount
        vput(resVNodeDir);

        return code;
}

/*
 * Similar to do_rmdir, but for files.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EPERM
 *        path refers to a directory.
 *      o ENOENT
 *        Any component in path does not exist, including the element at the
 *        very end.
 *      o ENOTDIR
 *        A component used as a directory in path is not, in fact, a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
int
do_unlink(const char *path)
{
        // NOT_YET_IMPLEMENTED("VFS: do_unlink");

        size_t namelen = 0;
        const char* name = NULL;
        vnode_t* resVNodeDir = NULL;

        // resolve parent directory of last file
        int code = dir_namev(path, &namelen, &name, NULL, &resVNodeDir);
        // if code < 0, dir_namev() fails
        // case: -ENOENT, -ENOTDIR, and -ENAMETOOLONG
        if(code < 0){
                return code;
        }

        vnode_t* resVNode = NULL;
        // lookup last file
        code = lookup(resVNodeDir, name, namelen, &resVNode);
        // if code < 0, lookup() fails
        // case: -ENOTDIR and -ENOENT
        if(code < 0){
                vput(resVNodeDir);
                return code;
        }

        // check -EPERM
        if(S_ISDIR(resVNode->vn_mode)){
                vput(resVNode);
                vput(resVNodeDir);
                return -EPERM;
        }

        // dir_vnode is the directory vnode where you will unlink the target file
        KASSERT(NULL != resVNodeDir->vn_ops->unlink);

        // call unlink() of resVNodeDir
        code = resVNodeDir->vn_ops->unlink(resVNodeDir, name, namelen);
        // decrement ref count of resVNode since we called lookup()
        vput(resVNode);
        vput(resVNodeDir);

        return code;
}

/* To link:
 *      o open_namev(from)
 *      o dir_namev(to)
 *      o call the destination dir's (to) link vn_ops.
 *      o return the result of link, or an error
 *
 * Remember to vput the vnodes returned from open_namev and dir_namev.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EEXIST
 *        to already exists.
 *      o ENOENT
 *        A directory component in from or to does not exist.
 *      o ENOTDIR
 *        A component used as a directory in from or to is not, in fact, a
 *        directory.
 *      o ENAMETOOLONG
 *        A component of from or to was too long.
 *      o EPERM
 *        from is a directory.
 */
int
do_link(const char *from, const char *to)
{
        // NOT_YET_IMPLEMENTED("VFS: do_link");

        // open_namev(from)
        vnode_t* fromVNode = NULL;
        int code = open_namev(from, O_RDONLY, &fromVNode, NULL);
        // if code < 0, open_namev() fails
        // case: -ENOENT, -ENOTDIR, and -ENAMETOOLONG
        if(code < 0){
                return code;
        }

        // we have increased fromVNode vn_refcount in open_namev()
        // so we decrease fromVNode vn_refcount
        vput(fromVNode);

        // check -EPERM
        if(S_ISDIR(fromVNode->vn_mode)){
                return -EPERM;
        }

        vnode_t* toVNodeDir = NULL;
        size_t namelen = 0;
        const char* name = NULL;

        // dir_namev(to)
        code = dir_namev(to, &namelen, &name, NULL, &toVNodeDir);
        // if code < 0, dir_namev() fails
        // case: -ENOENT, -ENOTDIR, and -ENAMETOOLONG
        if(code < 0){
                return code;
        }

         vnode_t* toVNode = NULL;
        // lookup toVNode
        code = lookup(toVNodeDir, name, namelen, &toVNode);
        // if code == 0, toVNode exists
        if(code == 0){
                // we have increased toVNode vn_refcount in lookup()
                // so we decrease toVNode vn_refcount
                vput(toVNode);
                vput(toVNodeDir);
                return -EEXIST;
        }

        // check link() of toVNodeDir
        if(NULL == toVNodeDir->vn_ops->link){
                vput(toVNodeDir);
                return -ENOTDIR;
        }
        // call link() of toVNodeDir
        code = toVNodeDir->vn_ops->link(fromVNode, toVNodeDir, name, namelen);
        vput(toVNodeDir);

        return code;
}

/*      o link newname to oldname
 *      o unlink oldname
 *      o return the value of unlink, or an error
 *
 * Note that this does not provide the same behavior as the
 * Linux system call (if unlink fails then two links to the
 * file could exist).
 */
int
do_rename(const char *oldname, const char *newname)
{
        // NOT_YET_IMPLEMENTED("VFS: do_rename");

        // link newname to oldname
        int code = do_link(oldname, newname);
        // if code < 0, do_link() fails
        if(code < 0){
                return code;
        }

        // unlink oldname
        code = do_unlink(oldname);
        // if code < 0, do_unlink() fails
        if(code < 0){
                return code;
        }

        return code;
}

/* Make the named directory the current process's cwd (current working
 * directory).  Don't forget to down the refcount to the old cwd (vput()) and
 * up the refcount to the new cwd (open_namev() or vget()). Return 0 on
 * success.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o ENOENT
 *        path does not exist.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 *      o ENOTDIR
 *        A component of path is not a directory.
 */
int
do_chdir(const char *path)
{
        // NOT_YET_IMPLEMENTED("VFS: do_chdir");

        vnode_t* resVNode = NULL;

        // resolve path
        int code = open_namev(path, O_RDONLY, &resVNode, NULL);
        // if code < 0, open_namev() fails
        // case: -ENOENT, -ENOTDIR, and -ENAMETOOLONG
        if(code < 0){
                return code;
        }
        // if code == 0, open_namev() succeeds
        // we have increased resVNode vn_refcount in open_namev()

        //check if file is a directory
        if(resVNode->vn_ops->lookup == NULL){
                vput(resVNode);
                return -ENOTDIR;
        }

        // decrease old cwd vn_refcount
        vput(curproc->p_cwd);
        // change cwd to new cwd
        curproc->p_cwd = resVNode;

        return 0;
}

/* Call the readdir vn_op on the given fd, filling in the given dirent_t*.
 * If the readdir vn_op is successful, it will return a positive value which
 * is the number of bytes copied to the dirent_t.  You need to increment the
 * file_t's f_pos by this amount.  As always, be aware of refcounts, check
 * the return value of the fget and the virtual function, and be sure the
 * virtual function exists (is not null) before calling it.
 *
 * Return either 0 or sizeof(dirent_t), or -errno.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        Invalid file descriptor fd.
 *      o ENOTDIR
 *        File descriptor does not refer to a directory.
 */
int
do_getdent(int fd, struct dirent *dirp)
{
        // NOT_YET_IMPLEMENTED("VFS: do_getdent");

        // check fd
        if(fd < 0 || fd >= NFILES || NULL == curproc->p_files[fd]){
                return -EBADF;
        }

        // fget
        file_t* file = fget(fd);

        // check -ENOTDIR
        if(NULL == file->f_vnode->vn_ops->readdir){
                // fput
                fput(file);
                return -ENOTDIR;
        }
        // call readdir() of file VNode
        int readByteSize = file->f_vnode->vn_ops->readdir(file->f_vnode, file->f_pos, dirp);
        // increase file f_pos
        file->f_pos += readByteSize;

        // fput
        fput(file);

        // if readByteSize == 0, f_pos has reached RAMFS_MAX_DIRENT
        if(0 == readByteSize){
                return 0;
        }

        // if readByteSize > 0
        return sizeof(*dirp);
}

/*
 * Modify f_pos according to offset and whence.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd is not an open file descriptor.
 *      o EINVAL
 *        whence is not one of SEEK_SET, SEEK_CUR, SEEK_END; or the resulting
 *        file offset would be negative.
 */
int
do_lseek(int fd, int offset, int whence)
{
        // NOT_YET_IMPLEMENTED("VFS: do_lseek");

        // check fd
        if(fd < 0){
                return -EBADF;
        }

        // fget
        file_t *file = fget(fd);
        // check file
        if(file == NULL){
                return -EBADF;
        }

        int file_size = file->f_vnode->vn_len;
        // modify f_pos according to offset and whence
        switch (whence)
        {
                case SEEK_SET:
                        if(offset < 0){
                                fput(file);
                                return -EINVAL;
                        }
                        file->f_pos = offset;
                        break;

                case SEEK_CUR:
                        if(file->f_pos + offset < 0){
                                fput(file);
                                return -EINVAL;
                        }
                        file->f_pos += offset;
                        break;

                case SEEK_END:
                        if(offset + file_size < 0){
                                fput(file);
                                return -EINVAL;
                        }
                        file->f_pos = file_size + offset;
                        break;

                default:
                        fput(file);
                        return -EINVAL;
        }

        fput(file);

        return file->f_pos;
}

/*
 * Find the vnode associated with the path, and call the stat() vnode operation.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o ENOENT
 *        A component of path does not exist.
 *      o ENOTDIR
 *        A component of the path prefix of path is not a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 *      o EINVAL
 *        path is an empty string.
 */
int
do_stat(const char *path, struct stat *buf)
{
        // NOT_YET_IMPLEMENTED("VFS: do_stat");

        // check empty string
        if(strlen(path) == 0){
                return -EINVAL;
        }

        // resolve path
        vnode_t* resVNode = NULL;
        int code = open_namev(path, O_RDONLY, &resVNode, NULL);
        // if code < 0, open_namev() fails
        // case: -ENOENT, -ENOTDIR, and -ENAMETOOLONG
        if(code < 0){
                return code;
        }

        // vn is the vnode where you will perform "stat"
        KASSERT(NULL != resVNode->vn_ops->stat);

        // call stat() of resVNode
        code = resVNode->vn_ops->stat(resVNode, buf);

        // we increase resVNode vn_refcount in open_namev()
        // but we only want to get stat of resVNode
        // so we decrease resVNode vn_refcount
        vput(resVNode);

        return code;
}



#ifdef __MOUNTING__
/*
 * Implementing this function is not required and strongly discouraged unless
 * you are absolutely sure your Weenix is perfect.
 *
 * This is the syscall entry point into vfs for mounting. You will need to
 * create the fs_t struct and populate its fs_dev and fs_type fields before
 * calling vfs's mountfunc(). mountfunc() will use the fields you populated
 * in order to determine which underlying filesystem's mount function should
 * be run, then it will finish setting up the fs_t struct. At this point you
 * have a fully functioning file system, however it is not mounted on the
 * virtual file system, you will need to call vfs_mount to do this.
 *
 * There are lots of things which can go wrong here. Make sure you have good
 * error handling. Remember the fs_dev and fs_type buffers have limited size
 * so you should not write arbitrary length strings to them.
 */
int
do_mount(const char *source, const char *target, const char *type)
{
        NOT_YET_IMPLEMENTED("MOUNTING: do_mount");
        return -EINVAL;
}

/*
 * Implementing this function is not required and strongly discouraged unless
 * you are absolutley sure your Weenix is perfect.
 *
 * This function delegates all of the real work to vfs_umount. You should not worry
 * about freeing the fs_t struct here, that is done in vfs_umount. All this function
 * does is figure out which file system to pass to vfs_umount and do good error
 * checking.
 */
int
do_umount(const char *target)
{
        NOT_YET_IMPLEMENTED("MOUNTING: do_umount");
        return -EINVAL;
}
#endif
