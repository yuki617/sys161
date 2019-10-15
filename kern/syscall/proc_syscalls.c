#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include "opt-A2.h"
#include <synch.h>
#include <machine/trapframe.h>
#include <copyinout.h>



  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  //(void)exitcode;
  pid_setexitstatus(p->p_pid, exitcode);
  pid_setisexited(p->p_pid, true);

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  struct semaphore *pid_sem = pid_getsem(p->p_pid);
  #if OPT_A2
      V(pid_sem);
  #endif
  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  *retval = 1;
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  if (options != 0) {
    return(EINVAL);
  }
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}

void thread_fork_init(void * data1, unsigned long data2){
    //Stop the compiler from complaining
    (void)data2;

    enter_forked_process((struct trapframe *) data1);

}

pid_t sys__fork(struct trapframe *ctf, pid_t *retval) {
  //kprintf("sys_fork could not create new address space\n");
  struct trapframe *tf_child = kmalloc(sizeof(struct trapframe));
  if (tf_child == NULL){
    return ENOMEM;
  }
  *tf_child = *ctf;

  struct proc *childproc = proc_create_runprogram(curproc->p_name);
  struct proc *p = curproc;
  pid_setparentpid(childproc->p_pid, curproc->p_pid);
  //changeparentpid(curproc);
  if(childproc == NULL){
    kfree(tf_child);
    return ENOMEM;
  } 
  //kprintf("enter spinlock \n");
  //spinlock_acquire(&childproc->p_lock);
  //kprintf("sys_fork could not create new address space\n");
  int res = as_copy(p->p_addrspace,&childproc->p_addrspace);
  if(res!=0){
    kfree(tf_child);
    proc_destroy(childproc);
    return ENOMEM;
  }
  // lock_acquire(p->wait_lock);
  // childproc->parent = curproc;
  // array_add(p->children,childproc,NULL);
  // lock_release(p->wait_lock);
  pid_t result = thread_fork(p->p_name,childproc,thread_fork_init,tf_child,0);
  //kprintf("gdb 1 \n");
  if(result != 0){
    as_destroy(childproc->p_addrspace);
    proc_destroy(childproc);
    kfree(tf_child);
    DEBUG(DB_SYSCALL, "thread forking failed");
    return ENOMEM;
  }
  //kprintf("three");
  //DEBUG(DB_SYSCALL, "three");
  *retval = childproc->p_pid;
  //kprintf("gdb 3 \n");
  return 0;
}
