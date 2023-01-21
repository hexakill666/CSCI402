
#include "kernel.h"
#include "globals.h"
#include "errno.h"
#include "types.h"

#include "main/interrupt.h"

#include "proc/proc.h"
#include "proc/kthread.h"

#include "util/init.h"
#include "util/string.h"
#include "util/debug.h"
#include "util/list.h"

#include "mm/mman.h"
#include "mm/mm.h"
#include "mm/page.h"
#include "mm/pframe.h"
#include "mm/kmalloc.h"

#include "fs/vfs_syscall.h"
#include "fs/vnode.h"

#include "test/kshell/kshell.h"

#include "vm/brk.h"
#include "vm/mmap.h"
#include "vm/vmmap.h"

#include "api/syscall.h"
#include "api/utsname.h"
#include "api/access.h"
#include "api/exec.h"

static void syscall_handler(regs_t *regs);
static int syscall_dispatch(uint32_t sysnum, uint32_t args, regs_t *regs);

static __attribute__((unused)) void syscall_init(void)
{
        intr_register(INTR_SYSCALL, syscall_handler);
}
init_func(syscall_init);

/*
 * this is one of the few sys_* functions you have to write. be sure to
 * check out the sys_* functions we have provided before trying to write
 * this one.
 *  - copy_from_user() the read_args_t
 *  - page_alloc() a temporary buffer
 *  - call do_read(), and copy_to_user() the read bytes
 *  - page_free() your buffer
 *  - return the number of bytes actually read, or if anything goes wrong
 *    set curthr->kt_errno and return -1
 */
static int
sys_read(read_args_t *arg)
{
        // NOT_YET_IMPLEMENTED("VM: sys_read");

        read_args_t kern_args;
        int readbytes;
        int err;

        // copy_from_user() the read_args_t
        err = copy_from_user(&kern_args, arg, sizeof(read_args_t));
        if (err < 0) {
                curthr->kt_errno = -err;
                return -1;
        }

        // page_alloc() a temporary buffer
        kern_args.buf = page_alloc();

        // call do_read()
        if((readbytes = do_read(kern_args.fd, kern_args.buf, kern_args.nbytes)) < 0){
                // if read fails, we should page_free() buffer here
                page_free(kern_args.buf);
                curthr->kt_errno = -readbytes;
                return -1;
        }

        // copy_to_user() the read bytes
        err = copy_to_user(arg->buf, kern_args.buf, readbytes);
        if (err < 0) {
                // maybe not reachable
                page_free(kern_args.buf);
                curthr->kt_errno = -err;
                return -1;
        }

        // page_free() your buffer
        page_free(kern_args.buf);

        return readbytes;
}

/*
 * This function is almost identical to sys_read.  See comments above.
 */
static int
sys_write(write_args_t *arg)
{
        // NOT_YET_IMPLEMENTED("VM: sys_write");

        write_args_t kern_args;
        int writebytes;
        int err;

        // copy_from_user() the write_args_t
        copy_from_user(&kern_args, arg, sizeof(write_args_t));
        if ((err = copy_from_user(&kern_args, arg, sizeof(write_args_t))) < 0) {
                curthr->kt_errno = -err;
                return -1;
        }

        // page_alloc() a temporary buffer
        kern_args.buf = page_alloc();

        // copy the buffer from user to kernel
        copy_from_user(kern_args.buf, arg->buf, arg->nbytes);
        if ((err =  copy_from_user(kern_args.buf, arg->buf, arg->nbytes)) < 0) {
                page_free(kern_args.buf);
                curthr->kt_errno = -err;
                return -1;
        }

        // call do_write()
        if((writebytes = do_write(kern_args.fd, kern_args.buf, kern_args.nbytes)) < 0){
                // if write fails, we should page_free() buffer here
                page_free(kern_args.buf);
                curthr->kt_errno = -writebytes;
                return -1;
        }

        // page_free() your buffer
        page_free(kern_args.buf);

        return writebytes;
}

/*
 * This is another tricly sys_* function that you will need to write.
 * It's pretty similar to sys_read(), but you don't need
 * to allocate a whole page, just a single dirent_t. call do_getdent in a
 * loop until you have read getdent_args_t->count bytes (or an error
 * occurs).  You should note that count is the number of bytes in the
 * buffer, not the number of dirents, which means that you'll need to loop
 * a max of something like count / sizeof(dirent_t) times.
 */
static int
sys_getdents(getdents_args_t *arg)
{
        // NOT_YET_IMPLEMENTED("VM: sys_getdents");

        getdents_args_t kern_args;
        uint32_t bytes = 0;
        int err;

        // copy_from_user() the getdents_args_t
        if ((err = copy_from_user(&kern_args, arg, sizeof(getdents_args_t))) < 0) {
                curthr->kt_errno = -err;
                return -1;
        }

        // kmalloc() a temporary dirent_t
        kern_args.dirp = kmalloc(sizeof(dirent_t));

        // we need another pointer to restore the current position in *dirent_t
        dirent_t *user_dirp_tail = arg->dirp;

        // call do_getdent in a loop
        while(bytes < kern_args.count){
                int readByteSize = do_getdent(kern_args.fd, kern_args.dirp);
                // do_getdent() fails
                if(readByteSize < 0){
                        // we should free() the dirent_t here
                        kfree(kern_args.dirp);
                        curthr->kt_errno = -readByteSize;
                        return -1;
                }
                // f_pos has reached maximum, simply break here
                if(readByteSize == 0){
                        break;
                }

                // if we have actually read something, copy it to user_dirp_tail and increase its position
                if((err = copy_to_user(user_dirp_tail, kern_args.dirp, sizeof(dirent_t))) < 0){
                        kfree(kern_args.dirp);
                        curthr->kt_errno = -err;
                        return -1;
                }
                user_dirp_tail++;
                bytes += sizeof(dirent_t);
        }

        // free() the dirent_t
        kfree(kern_args.dirp);

        return bytes;
}

#ifdef __MOUNTING__
static int sys_mount(mount_args_t *arg)
{
        mount_args_t kern_args;
        char *source;
        char *target;
        char *type;
        int ret;

        if (copy_from_user(&kern_args, arg, sizeof(kern_args)) < 0) {
                curthr->kt_errno = EFAULT;
                return -1;
        }

        /* null is okay only for the source */
        source = user_strdup(&kern_args.spec);
        if (NULL == (target = user_strdup(&kern_args.dir))) {
                kfree(source);
                curthr->kt_errno = EINVAL;
                return -1;
        }
        if (NULL == (type = user_strdup(&kern_args.fstype))) {
                kfree(source);
                kfree(target);
                curthr->kt_errno = EINVAL;
                return -1;
        }

        ret = do_mount(source, target, type);
        kfree(source);
        kfree(target);
        kfree(type);

        if (ret) {
                curthr->kt_errno = -ret;
                return -1;
        }

        return 0;
}

static int sys_umount(argstr_t *input)
{
        argstr_t kstr;
        char *target;
        int ret;

        if (copy_from_user(&kstr, input, sizeof(kstr)) < 0) {
                curthr->kt_errno = EFAULT;
                return -1;
        }

        if (NULL == (target = user_strdup(&kstr))) {
                curthr->kt_errno = EINVAL;
                return -1;
        }

        ret = do_umount(target);
        kfree(target);

        if (ret) {
                curthr->kt_errno = -ret;
                return -1;
        }

        return 0;
}
#endif

static int sys_close(int fd)
{
        int err;

        err = do_close(fd);
        if (err < 0) {
                curthr->kt_errno = -err;
                return -1;
        } else return err;
}

static int sys_dup(int fd)
{
        int err;

        if ((err = do_dup(fd)) < 0) {
                curthr->kt_errno = -err;
                return -1;
        } else return err;
}

static int sys_dup2(const dup2_args_t *arg)
{
        dup2_args_t             kern_args;
        int                     err;

        if ((err = copy_from_user(&kern_args, arg, sizeof(kern_args))) < 0) {
                curthr->kt_errno = -err;
                return -1;
        }

        if ((err = do_dup2(kern_args.ofd, kern_args.nfd)) < 0) {
                curthr->kt_errno = -err;
                return -1;
        } else return err;
}

static int sys_mkdir(mkdir_args_t *arg)
{
        mkdir_args_t            kern_args;
        char                   *path;
        int                     err;

        if ((err = copy_from_user(&kern_args, arg, sizeof(mkdir_args_t))) < 0) {
                curthr->kt_errno = -err;
                return -1;
        }

        path = user_strdup(&kern_args.path);
        if (!path) {
                curthr->kt_errno = EINVAL;
                return -1;
        }

        err = do_mkdir(path);
        kfree(path);
        if (err < 0) {
                curthr->kt_errno = -err;
                return -1;
        } else return err;
}

static int sys_rmdir(argstr_t *arg)
{
        argstr_t                kern_args;
        char                   *path;
        int                     err;

        if ((err = copy_from_user(&kern_args, arg, sizeof(argstr_t))) < 0) {
                curthr->kt_errno = -err;
                return -1;
        }
        path = user_strdup(&kern_args);

        if (!path) {
                curthr->kt_errno = EINVAL;
                return -1;
        }

        err = do_rmdir(path);
        kfree(path);
        if (err < 0) {
                curthr->kt_errno = -err;
                return -1;
        } else return err;
}

static int sys_unlink(argstr_t *arg)
{
        argstr_t                kern_args;
        char                    *path;
        int                     err;

        if ((err = copy_from_user(&kern_args, arg, sizeof(argstr_t))) < 0) {
                curthr->kt_errno = -err;
                return -1;
        }

        path = user_strdup(&kern_args);
        if (!path) {
                curthr->kt_errno = EINVAL;
                return -1;
        }

        err = do_unlink(path);
        kfree(path);
        if (err < 0) {
                curthr->kt_errno = -err;
                return -1;
        } else return err;
}

static int sys_link(link_args_t *arg)
{
        link_args_t             kern_args;
        char                    *to;
        char                    *from;
        int                     err;

        if ((err = copy_from_user(&kern_args, arg, sizeof(link_args_t))) < 0) {
                curthr->kt_errno = -err;
                return -1;
        }

        to = user_strdup(&kern_args.to);
        if (!to) {
                curthr->kt_errno = EINVAL;
                return -1;
        }

        from = user_strdup(&kern_args.from);
        if (!from) {
                curthr->kt_errno = EINVAL;
                kfree(to);
                return -1;
        }

        err = do_link(from, to);
        kfree(to);
        kfree(from);

        if (err < 0) {
                curthr->kt_errno = -err;
                return -1;
        } else {
                return err;
        }
}

static int sys_rename(rename_args_t *arg)
{
        rename_args_t           kern_args;
        char                    *oldname;
        char                    *newname;
        int                     err;

        if ((err = copy_from_user(&kern_args, arg, sizeof(rename_args_t))) < 0) {
                curthr->kt_errno = -err;
                return -1;
        }

        oldname = user_strdup(&kern_args.oldname);
        if (!oldname) {
                curthr->kt_errno = EINVAL;
                return -1;
        }

        newname = user_strdup(&kern_args.newname);
        if (!newname) {
                curthr->kt_errno = EINVAL;
                kfree(oldname);
                return -1;
        }

        err = do_rename(oldname, newname);
        kfree(newname);
        kfree(oldname);

        if (err < 0) {
                curthr->kt_errno = -err;
                return -1;
        } else return err;
}

static int sys_chdir(argstr_t *arg)
{
        argstr_t        kern_args;
        char            *path;
        int             err;

        if ((err = copy_from_user(&kern_args, arg, sizeof(argstr_t))) < 0) {
                curthr->kt_errno = -err;
                return -1;
        }

        path = user_strdup(&kern_args);
        if (!path) {
                curthr->kt_errno = EINVAL;
                return -1;
        }

        err = do_chdir(path);
        kfree(path);

        if (err < 0) {
                curthr->kt_errno = -err;
                return -1;
        } else return err;
}

static int sys_lseek(lseek_args_t *args)
{
        lseek_args_t            kargs;
        int                     err;

        if ((err = copy_from_user(&kargs, args, sizeof(lseek_args_t))) < 0) {
                curthr->kt_errno = -err;
                return -1;
        }

        err = do_lseek(kargs.fd, kargs.offset, kargs.whence);

        if (err < 0) {
                curthr->kt_errno = -err;
                return -1;
        } else return err;
}

static int sys_open(open_args_t *arg)
{
        open_args_t             kern_args;
        char                    *path;
        int                     err;

        if ((err = copy_from_user(&kern_args, arg, sizeof(open_args_t))) < 0) {
                curthr->kt_errno = -err;
                return -1;
        }

        path = user_strdup(&kern_args.filename);
        if (!path) {
                curthr->kt_errno = EINVAL;
                return -1;
        }

        err = do_open(path, kern_args.flags);
        kfree(path);
        if (err < 0) {
                curthr->kt_errno = -err;
                return -1;
        } else return err;
}

static int sys_munmap(munmap_args_t *args)
{
        munmap_args_t           kargs;
        int                     err;

        if (copy_from_user(&kargs, args, sizeof(munmap_args_t))) {
                curthr->kt_errno = EFAULT;
                return -1;
        }

        err = do_munmap(kargs.addr, kargs.len);
        if (err < 0) {
                curthr->kt_errno = -err;
                return -1;
        }
        return 0;
}

static void *sys_mmap(mmap_args_t *arg)
{
        mmap_args_t             kargs;
        void                    *ret;
        int                     err;

        if (copy_from_user(&kargs, arg, sizeof(mmap_args_t)) < 0) {
                curthr->kt_errno = EFAULT;
                return MAP_FAILED;
        }

        err = do_mmap(kargs.mma_addr, kargs.mma_len, kargs.mma_prot,
                      kargs.mma_flags, kargs.mma_fd, kargs.mma_off, &ret);
        if (err < 0) {
                curthr->kt_errno = -err;
                return MAP_FAILED;
        }
        return ret;
}


static pid_t sys_waitpid(waitpid_args_t *args)
{
        int s, p;
        waitpid_args_t kargs;

        if (0 > copy_from_user(&kargs, args, sizeof(kargs))) {
                curthr->kt_errno = EFAULT;
                return -1;
        }

        if (0 > (p = do_waitpid(kargs.wpa_pid, kargs.wpa_options, &s))) {
                curthr->kt_errno = -p;
                return -1;
        }

        if (NULL != kargs.wpa_status && 0 > copy_to_user(kargs.wpa_status, &s, sizeof(int))) {
                curthr->kt_errno = EFAULT;
                return -1;
        }

        return p;
}

static void *sys_brk(void *addr)
{
        void *ret;
        int err;

        if (0 == (err = do_brk(addr, &ret))) {
                return ret;
        } else {
                curthr->kt_errno = -err;
                return (void *) - 1;
        }
}

static void sys_sync(void)
{
        pframe_clean_all();
}

static void sys_halt(void)
{
        proc_kill_all();
}

static int sys_stat(stat_args_t *arg)
{
        stat_args_t kern_args;
        struct stat buf;
        char *path;
        int ret;

        if (copy_from_user(&kern_args, arg, sizeof(kern_args)) < 0) {
                curthr->kt_errno = EFAULT;
                return -1;
        }

        if ((path = user_strdup(&kern_args.path)) == NULL) {
                curthr->kt_errno = EINVAL;
                return -1;
        }

        ret = do_stat(path, &buf);

        if (ret == 0) {
                ret = copy_to_user(kern_args.buf, &buf, sizeof(struct stat));
        }

        if (ret != 0) {
                kfree(path);
                curthr->kt_errno = -ret;
                return -1;
        }

        kfree(path);
        return 0;
}

static int sys_pipe(int arg[2])
{
        int kern_args[2];
        int ret;

        ret = do_pipe(kern_args);

        if (ret == 0) {
                ret = copy_to_user(arg, kern_args, sizeof(kern_args));
        }

        if (ret != 0) {
                curthr->kt_errno = -ret;
                return -1;
        }

        return 0;
}

static int sys_uname(struct utsname *arg)
{
        static const char sysname[] = "Weenix";
        static const char release[] = "1.2";
        /* Version = last compilation time */
        static const char version[] = "#1 " __DATE__ " " __TIME__;
        static const char nodename[] = "";
        static const char machine[] = "";
        int ret = 0;

        ret = copy_to_user(arg->sysname, sysname, sizeof(sysname));
        if (ret != 0) {
                goto err;
        }
        ret = copy_to_user(arg->release, release, sizeof(release));
        if (ret != 0) {
                goto err;
        }
        ret = copy_to_user(arg->version, version, sizeof(version));
        if (ret != 0) {
                goto err;
        }
        ret = copy_to_user(arg->nodename, nodename, sizeof(nodename));
        if (ret != 0) {
                goto err;
        }
        ret = copy_to_user(arg->machine, machine, sizeof(machine));
        if (ret != 0) {
                goto err;
        }
        return 0;
err:
        curthr->kt_errno = -ret;
        return -1;
}

static int sys_fork(regs_t *regs)
{
        int ret = do_fork(regs);
        if (ret < 0) {
                curthr->kt_errno = -ret;
                return -1;
        }
        return ret;
}

static void free_vector(char **vect)
{
        char **temp;
        for (temp = vect; *temp; temp++)
                kfree(*temp);
        kfree(vect);
}

static int sys_execve(execve_args_t *args, regs_t *regs)
{
        execve_args_t kern_args;
        char *kern_filename = NULL;
        char **kern_argv = NULL;
        char **kern_envp = NULL;
        int err;

        if ((err = copy_from_user(&kern_args, args, sizeof(kern_args))) < 0) {
                curthr->kt_errno = -err;
                goto cleanup;
        }

        /* copy the name of the executable */
        if ((kern_filename = user_strdup(&kern_args.filename)) == NULL)
                goto cleanup;

        /* copy the argument list */
        if (kern_args.argv.av_vec) {
                if ((kern_argv = user_vecdup(&kern_args.argv)) == NULL)
                        goto cleanup;
        }

        /* copy the environment list */
        if (kern_args.envp.av_vec) {
                if ((kern_envp = user_vecdup(&kern_args.envp)) == NULL)
                        goto cleanup;
        }

        err = do_execve(kern_filename, kern_argv, kern_envp, regs);

        curthr->kt_errno = -err;

cleanup:
        if (kern_filename)
                kfree(kern_filename);
        if (kern_argv)
                free_vector(kern_argv);
        if (kern_envp)
                free_vector(kern_envp);
        if (curthr->kt_errno)
                return -1;
        return 0;
}

static int sys_debug(argstr_t *arg)
{
        argstr_t kern_args;
        int      err;
        char    *message;

        if ((err = copy_from_user(&kern_args, arg, sizeof(argstr_t))) < 0) {
                curthr->kt_errno = -err;
                return -1;
        }
        message = user_strdup(&kern_args);
        dbg(DBG_USER, "%s\n", message);

        kfree(message);
        return 0;
}

static int sys_kshell(int ttyid)
{
        kshell_t *ksh;
        int       err;

        /* Create a kshell on tty */
        ksh = kshell_create(ttyid);
        if (NULL == ksh) {
                curthr->kt_errno = ENODEV;
                return -1;
        }

        while ((err = kshell_execute_next(ksh)) > 0);
        kshell_destroy(ksh);
        if (err < 0) {
                curthr->kt_errno = -err;
                return -1;
        }

        return 0;
}

/* Interrupt handler for syscalls */
static void syscall_handler(regs_t *regs)
{

        /* The syscall number and the (user-address) pointer to the arguments.
         * Pushed by userland when we trap into the kernel */
        uint32_t sysnum = (uint32_t) regs->r_eax;
        uint32_t args = (uint32_t) regs->r_edx;

        dbg(DBG_SYSCALL, ">> pid %d, sysnum: %d (%x), arg: %d (%#08x)\n",
            curproc->p_pid, sysnum, sysnum, args, args);

        if (curthr->kt_cancelled) {
                dbg(DBG_SYSCALL, "trap: CANCELLING: thread %p of proc %d "
                    "(0x%p)\n", curthr, curproc->p_pid, curproc);

                kthread_exit(curthr->kt_retval);
        }

        dbginfo(DBG_VMMAP, vmmap_mapping_info, curproc->p_vmmap);

        int ret = syscall_dispatch(sysnum, args, regs);

        if (curthr->kt_cancelled) {
                dbg(DBG_SYSCALL, "trap: CANCELLING: thread %p of proc %d "
                    "(%p)\n", curthr, curproc->p_pid, curproc);

                kthread_exit(curthr->kt_retval);
        }

        dbg(DBG_SYSCALL, "<< pid %d, sysnum: %d (%x), returned: %d (%#x)\n",
            curproc->p_pid, sysnum, sysnum, ret, ret);
        regs->r_eax = ret; /* Return value goes in eax */
}

static int syscall_dispatch(uint32_t sysnum, uint32_t args, regs_t *regs)
{
        switch (sysnum) {
                case SYS_waitpid:
                        return sys_waitpid((waitpid_args_t *)args);

                case SYS_exit:
                        do_exit((int)args);
                        panic("exit failed!\n");
                        return 0;

                case SYS_thr_exit:
                        kthread_exit((void *)args);
                        panic("thr_exit failed!\n");
                        return 0;

                case SYS_thr_yield:
                        sched_make_runnable(curthr);
                        sched_switch();
                        return 0;

                case SYS_fork:
                        return sys_fork(regs);

                case SYS_getpid:
                        return curproc->p_pid;

                case SYS_sync:
                        sys_sync();
                        return 0;

#ifdef __MOUNTING__
                case SYS_mount:
                        return sys_mount((mount_args_t *) args);

                case SYS_umount:
                        return sys_umount((argstr_t *) args);
#endif

                case SYS_mmap:
                        return (int) sys_mmap((mmap_args_t *) args);

                case SYS_munmap:
                        return sys_munmap((munmap_args_t *) args);

                case SYS_open:
                        return sys_open((open_args_t *) args);

                case SYS_close:
                        return sys_close((int)args);

                case SYS_read:
                        return sys_read((read_args_t *)args);

                case SYS_write:
                        return sys_write((write_args_t *)args);

                case SYS_dup:
                        return sys_dup((int)args);

                case SYS_dup2:
                        return sys_dup2((dup2_args_t *)args);

                case SYS_mkdir:
                        return sys_mkdir((mkdir_args_t *)args);

                case SYS_rmdir:
                        return sys_rmdir((argstr_t *)args);

                case SYS_unlink:
                        return sys_unlink((argstr_t *)args);

                case SYS_link:
                        return sys_link((link_args_t *)args);

                case SYS_rename:
                        return sys_rename((rename_args_t *)args);

                case SYS_chdir:
                        return sys_chdir((argstr_t *)args);

                case SYS_getdents:
                        return sys_getdents((getdents_args_t *)args);

                case SYS_brk:
                        return (int) sys_brk((void *)args);

                case SYS_lseek:
                        return sys_lseek((lseek_args_t *)args);

                case SYS_halt:
                        sys_halt();
                        return -1;

                case SYS_set_errno:
                        curthr->kt_errno = (int)args;
                        return 0;

                case SYS_errno:
                        return curthr->kt_errno;

                case SYS_execve:
                        return sys_execve((execve_args_t *)args, regs);

                case SYS_stat:
                        return sys_stat((stat_args_t *)args);

                case SYS_pipe:
                        return sys_pipe((int *)args);

                case SYS_uname:
                        return sys_uname((struct utsname *)args);

                case SYS_debug:
                        return sys_debug((argstr_t *)args);
                case SYS_kshell:
                        return sys_kshell((int)args);
                default:
                        dbg(DBG_ERROR, "ERROR: unknown system call: %d (args: %#08x)\n", sysnum, args);
                        curthr->kt_errno = ENOSYS;
                        return -1;
        }
}

