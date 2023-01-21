
#include "types.h"
#include "globals.h"
#include "kernel.h"
#include "errno.h"

#include "util/gdb.h"
#include "util/init.h"
#include "util/debug.h"
#include "util/string.h"
#include "util/printf.h"

#include "mm/mm.h"
#include "mm/page.h"
#include "mm/pagetable.h"
#include "mm/pframe.h"

#include "vm/vmmap.h"
#include "vm/shadowd.h"
#include "vm/shadow.h"
#include "vm/anon.h"

#include "main/acpi.h"
#include "main/apic.h"
#include "main/interrupt.h"
#include "main/gdt.h"

#include "proc/sched.h"
#include "proc/proc.h"
#include "proc/kthread.h"

#include "drivers/dev.h"
#include "drivers/blockdev.h"
#include "drivers/disk/ata.h"
#include "drivers/tty/virtterm.h"
#include "drivers/pci.h"

#include "api/exec.h"
#include "api/syscall.h"

#include "fs/vfs.h"
#include "fs/vnode.h"
#include "fs/vfs_syscall.h"
#include "fs/fcntl.h"
#include "fs/stat.h"

#include "test/kshell/kshell.h"
#include "test/s5fs_test.h"

GDB_DEFINE_HOOK(boot)
GDB_DEFINE_HOOK(initialized)
GDB_DEFINE_HOOK(shutdown)

static void      *bootstrap(int arg1, void *arg2);
static void      *idleproc_run(int arg1, void *arg2);
static kthread_t *initproc_create(void);
static void      *initproc_run(int arg1, void *arg2);
static void       hard_shutdown(void);

static context_t bootstrap_context;
extern int gdb_wait;

extern void *faber_thread_test(int arg1, void *arg2);
extern void *sunghan_test(int arg1, void *arg2);
extern void *sunghan_deadlock_test(int arg1, void *arg2);

extern int vfstest_main(int arg1, char **arg2);
extern int faber_fs_thread_test(kshell_t *ksh, int argc, char **argv);
extern int faber_directory_test(kshell_t *ksh, int argc, char **argv);

typedef struct {
    struct proc *p;
    struct kthread *t;
} proc_thread_t;

#ifdef __DRIVERS__

int my_sunghan_test(kshell_t *kshell, int argc, char **argv)
{
    KASSERT(kshell != NULL);
    proc_thread_t new_pt;
    new_pt.p = proc_create("sunghantest");
    new_pt.t = kthread_create(new_pt.p, sunghan_test, 0, NULL);
    sched_make_runnable(new_pt.t);
    while(!list_empty(&curproc->p_children)){
        do_waitpid(-1, 0, NULL);
    }
    return 0;
}

int my_sunghan_deadlock_test(kshell_t *kshell, int argc, char **argv)
{
    KASSERT(kshell != NULL);
    proc_thread_t new_pt;
    new_pt.p = proc_create("sunghandeadlocktest");
    new_pt.t = kthread_create(new_pt.p, sunghan_deadlock_test, 0, NULL);
    sched_make_runnable(new_pt.t);
    while(!list_empty(&curproc->p_children)){
        do_waitpid(-1, 0, NULL);
    }
    return 0;
}

int my_faber_thread_test(kshell_t *kshell, int argc, char **argv)
{
    KASSERT(kshell != NULL);
    proc_thread_t new_pt;
    new_pt.p = proc_create("faberthreadtest");
    new_pt.t = kthread_create(new_pt.p, faber_thread_test, 0, NULL);
    sched_make_runnable(new_pt.t);
    while(!list_empty(&curproc->p_children)){
        do_waitpid(-1, 0, NULL);
    }
    return 0;
}

#ifdef __VFS__

int my_vfs_test(kshell_t *kshell, int argc, char **argv)
{
    KASSERT(kshell != NULL);
    proc_thread_t new_pt;
    new_pt.p = proc_create("vfstest");
    new_pt.t = kthread_create(new_pt.p, (void*)vfstest_main, 1, NULL);
    sched_make_runnable(new_pt.t);
    while(!list_empty(&curproc->p_children)){
        do_waitpid(-1, 0, NULL);
    }
    return 0;
}

#endif /* __VFS__ */

#endif /* __DRIVERS__ */

/**
 * This is the first real C function ever called. It performs a lot of
 * hardware-specific initialization, then creates a pseudo-context to
 * execute the bootstrap function in.
 */
void
kmain()
{
        GDB_CALL_HOOK(boot);

        dbg_init();
        dbgq(DBG_CORE, "Kernel binary:\n");
        dbgq(DBG_CORE, "  text: 0x%p-0x%p\n", &kernel_start_text, &kernel_end_text);
        dbgq(DBG_CORE, "  data: 0x%p-0x%p\n", &kernel_start_data, &kernel_end_data);
        dbgq(DBG_CORE, "  bss:  0x%p-0x%p\n", &kernel_start_bss, &kernel_end_bss);

        page_init();

        pt_init();
        slab_init();
        pframe_init();

        acpi_init();
        apic_init();
        pci_init();
        intr_init();

        gdt_init();

        /* initialize slab allocators */
#ifdef __VM__
        anon_init();
        shadow_init();
#endif
        vmmap_init();
        proc_init();
        kthread_init();

#ifdef __DRIVERS__
        bytedev_init();
        blockdev_init();
#endif

        void *bstack = page_alloc();
        pagedir_t *bpdir = pt_get();
        KASSERT(NULL != bstack && "Ran out of memory while booting.");
        /* This little loop gives gdb a place to synch up with weenix.  In the
         * past the weenix command started qemu was started with -S which
         * allowed gdb to connect and start before the boot loader ran, but
         * since then a bug has appeared where breakpoints fail if gdb connects
         * before the boot loader runs.  See
         *
         * https://bugs.launchpad.net/qemu/+bug/526653
         *
         * This loop (along with an additional command in init.gdb setting
         * gdb_wait to 0) sticks weenix at a known place so gdb can join a
         * running weenix, set gdb_wait to zero  and catch the breakpoint in
         * bootstrap below.  See Config.mk for how to set GDBWAIT correctly.
         *
         * DANGER: if GDBWAIT != 0, and gdb is not running, this loop will never
         * exit and weenix will not run.  Make SURE the GDBWAIT is set the way
         * you expect.
         */
        while (gdb_wait) ;
        context_setup(&bootstrap_context, bootstrap, 0, NULL, bstack, PAGE_SIZE, bpdir);
        context_make_active(&bootstrap_context);

        panic("\nReturned to kmain()!!!\n");
}

/**
 * Clears all interrupts and halts, meaning that we will never run
 * again.
 */
static void
hard_shutdown()
{
#ifdef __DRIVERS__
        vt_print_shutdown();
#endif
        __asm__ volatile("cli; hlt");
}

/**
 * This function is called from kmain, however it is not running in a
 * thread context yet. It should create the idle process which will
 * start executing idleproc_run() in a real thread context.  To start
 * executing in the new process's context call context_make_active(),
 * passing in the appropriate context. This function should _NOT_
 * return.
 *
 * Note: Don't forget to set curproc and curthr appropriately.
 *
 * @param arg1 the first argument (unused)
 * @param arg2 the second argument (unused)
 */

// Weenix Doc:
//      The goal of bootstrap() is to set up the first kernel
//      thread and process, which together are called the idle process, and execute its
//      main routine. This should be a piece of cake once you have implemented threads
//      and the scheduler.
//      The idle process performs further initialization and starts the init process.
//      For now, all of your test code should be run in the init process.

static void *
bootstrap(int arg1, void *arg2)
{
        /* If the next line is removed/altered in your submission, 20 points will be deducted. */
        dbgq(DBG_TEST, "SIGNATURE: 53616c7465645f5f78a899835fb2a52bc7ada9b6a74d4717e942ad14c4d51b8746fcb742b16dc63c2ea3d123c9046292\n");
        /* necessary to finalize page table information */
        pt_template_init();

        // NOT_YET_IMPLEMENTED("PROCS: bootstrap");

        proc_thread_t idle_pt;

        // create idle process and 
        idle_pt.p = proc_create("idle proc");
        idle_pt.t = kthread_create(idle_pt.p, idleproc_run, 0, NULL);

        // set current thread and process
        curproc = idle_pt.p;
        curthr = idle_pt.t;

        // curproc was uninitialized before, it is initialized here to point to the "idle" process
        KASSERT(NULL != curproc);
        // make sure the process ID of the created "idle" process is PID_IDLE
        KASSERT(PID_IDLE == curproc->p_pid);
        // curthr was uninitialized before, it is initialized here to point to the thread of the "idle" process
        KASSERT(NULL != curthr);
        
        // execute idle process
        context_make_active(&idle_pt.t->kt_ctx);

        panic("weenix returned to bootstrap()!!! BAD!!!\n");

        return NULL;
}

/**
 * Once we're inside of idleproc_run(), we are executing in the context of the
 * first process-- a real context, so we can finally begin running
 * meaningful code.
 *
 * This is the body of process 0. It should initialize all that we didn't
 * already initialize in kmain(), launch the init process (initproc_run),
 * wait for the init process to exit, then halt the machine.
 *
 * @param arg1 the first argument (unused)
 * @param arg2 the second argument (unused)
 */
static void *
idleproc_run(int arg1, void *arg2)
{
        int status;
        pid_t child;

        /* create init proc */
        kthread_t *initthr = initproc_create();
        init_call_all();
        GDB_CALL_HOOK(initialized);

        /* Create other kernel threads (in order) */

#ifdef __VFS__
        /* Once you have VFS remember to set the current working directory
         * of the idle and init processes */

        // NOT_YET_IMPLEMENTED("VFS: idleproc_run");

        // set idle process p_cwd VNode
        curproc->p_cwd = vfs_root_vn;
        // increase p_cwd VNode vn_refcount of idle process
        vref(curproc->p_cwd);

        // set init process p_cwd VNode
        initthr->kt_proc->p_cwd = vfs_root_vn;
        // increase p_cwd VNode vn_refcount of init process
        vref(initthr->kt_proc->p_cwd);

        /* Here you need to make the null, zero, and tty devices using mknod */
        /* You can't do this until you have VFS, check the include/drivers/dev.h
         * file for macros with the device ID's you will need to pass to mknod */
        
        // NOT_YET_IMPLEMENTED("VFS: idleproc_run");

        int devCode = do_mkdir("/dev");
        int nullCode = do_mknod("/dev/null", S_IFCHR, MEM_NULL_DEVID);
        int zeroCode = do_mknod("/dev/zero", S_IFCHR, MEM_ZERO_DEVID);
        int tty0Code = do_mknod("/dev/tty0", S_IFCHR, MKDEVID(2, 0));
        int tty1Code = do_mknod("/dev/tty1", S_IFCHR, MKDEVID(2, 1));
#endif

        /* Finally, enable interrupts (we want to make sure interrupts
         * are enabled AFTER all drivers are initialized) */
        intr_enable();

        /* Run initproc */
        sched_make_runnable(initthr);
        /* Now wait for it */
        child = do_waitpid(-1, 0, &status);
        KASSERT(PID_INIT == child);

#ifdef __MTP__
        kthread_reapd_shutdown();
#endif


#ifdef __SHADOWD__
        /* wait for shadowd to shutdown */
        shadowd_shutdown();
#endif

#ifdef __VFS__
        /* Shutdown the vfs: */
        dbg_print("weenix: vfs shutdown...\n");
        vput(curproc->p_cwd);
        if (vfs_shutdown())
                panic("vfs shutdown FAILED!!\n");

#endif

        /* Shutdown the pframe system */
#ifdef __S5FS__
        pframe_shutdown();
#endif

        dbg_print("\nweenix: halted cleanly!\n");
        GDB_CALL_HOOK(shutdown);
        hard_shutdown();
        return NULL;
}

/**
 * This function, called by the idle process (within 'idleproc_run'), creates the
 * process commonly refered to as the "init" process, which should have PID 1.
 *
 * The init process should contain a thread which begins execution in
 * initproc_run().
 *
 * @return a pointer to a newly created thread which will execute
 * initproc_run when it begins executing
 */
static kthread_t *
initproc_create(void)
{
        // NOT_YET_IMPLEMENTED("PROCS: initproc_create");

        proc_thread_t init_pt;
        init_pt.p = proc_create("init proc");
        init_pt.t = kthread_create(init_pt.p, initproc_run, 0, NULL);

        // p is the pointer to the "init" process, thr is the pointer to the thread of p
        KASSERT(NULL != init_pt.p);
        KASSERT(PID_INIT == init_pt.p->p_pid);
        KASSERT(NULL != init_pt.t);
        
        return init_pt.t;
}

/**
 * The init thread's function changes depending on how far along your Weenix is
 * developed. Before VM/FI, you'll probably just want to have this run whatever
 * tests you've written (possibly in a new process). After VM/FI, you'll just
 * exec "/sbin/init".
 *
 * Both arguments are unused.
 *
 * @param arg1 the first argument (unused)
 * @param arg2 the second argument (unused)
 */
//  the SELF-checks requirement does not apply to the code you wrote in initproc_run()
//  and the code you wrote for your kshell commands
static void *
initproc_run(int arg1, void *arg2)
{
        // NOT_YET_IMPLEMENTED("PROCS: initproc_run");

#ifdef __DRIVERS__

        kshell_add_command("sunghan", my_sunghan_test, "Run sunghan_test().");
        kshell_add_command("deadlock", my_sunghan_deadlock_test, "Run sunghan_deadlock_test().");
        kshell_add_command("faber", my_faber_thread_test, "Run faber_thread_test().");

#ifdef __VFS__

        kshell_add_command("vfstest", my_vfs_test, "Run vfstest_main().");
        kshell_add_command("fstest", faber_fs_thread_test, "Run faber_fs_thread_test().");
        kshell_add_command("dirtest", faber_directory_test, "Run faber_directory_test().");

#endif /* __VFS__ */

#ifdef __VM__

        // run init.c
        char *const argv[] = { NULL };
        char *const envp[] = { NULL };
        kernel_execve("/sbin/init", argv, envp);

#else

        kshell_t *kshell = kshell_create(0);
        while (kshell_execute_next(kshell)){
        }
        kshell_destroy(kshell);

#endif /* __VM__ */

#endif /* __DRIVERS__ */

        return NULL;
}
