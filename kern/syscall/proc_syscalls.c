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
#include <kern/fcntl.h>
#include <vfs.h>
#include <test.h>
#include <limits.h>


int execv(const char *program,char**args){
    char * program_kernel = kmalloc((strlen(program) + 1) * sizeof(char));
    vaddr_t entrypoint, stackptr;
    struct vnode *v;
    char **kernelargs;
    int arg_len =0;
    for(unsigned int i =0; args[i]!=NULL;i++){
        arg_len++;
    }
    kernelargs = kmalloc(sizeof(char *) * (arg_len+1));
    for(int i =0; i < arg_len;i++){
      kernelargs[i] = kmalloc((strlen(args[i])+1)* sizeof(char));
      copyin((const_userptr_t) args[i], kernelargs[i], (strlen(args[i])+1)*sizeof(char));
    }
    kernelargs[arg_len] =NULL;
    /* Open the file. */
    copyin((const_userptr_t) program, (void *) program_kernel, (strlen(program) + 1) * sizeof(char));
    int result = vfs_open(program_kernel,O_RDONLY, 0, &v);
    if (result) {
      return result;
    }

    /* Create a new address space. */
    struct addrspace * as = as_create();
    if (as ==NULL) {
      vfs_close(v);
      return ENOMEM;
    }

    /* Switch to it and activate it. */
    struct addrspace * oldas = curproc_setas(as);
    as_activate();

    result = load_elf(v, &entrypoint);
    if (result) {
      vfs_close(v);
      return result;
    }
    vfs_close(v);

        /* Define the user stack in the address space */
    result = as_define_stack(as, &stackptr);
    if (result) {
      /* p_addrspace will go away when curproc is destroyed */
      return result;
    }
    
    vaddr_t temp_stack_ptr = stackptr;
    vaddr_t *stack = kmalloc((arg_len + 1) * sizeof(vaddr_t));
    size_t actualarglen[arg_len];
    for (int i = 0; i <arg_len; i++){
      actualarglen[i] = strlen(kernelargs[i]) + 1;
    }
    for(int i =arg_len-1; i >=0; i--){
      temp_stack_ptr = temp_stack_ptr - ROUNDUP(actualarglen[i], 8);
	  //kprintf("%s",kernelArgs[i]);
      copyoutstr(kernelargs[i],(userptr_t) temp_stack_ptr,ARG_MAX, &actualarglen[i]);
      stack[i] = temp_stack_ptr;
    }
    stack[arg_len] =(vaddr_t)NULL;
    for(int i =arg_len; i >=0; i--){
      temp_stack_ptr = temp_stack_ptr - sizeof(vaddr_t);
      copyout(&stack[i],(userptr_t) temp_stack_ptr,sizeof(vaddr_t)) ;
    }
    
    as_destroy(oldas);

  vaddr_t userspace = 0;

  if(arg_len >=1){
      userspace = temp_stack_ptr;

  }
	/* Warp to user mode. */
	 enter_new_process(arg_len /*argc*/, (userptr_t) userspace /*userspace addr of argv*/,
			  ROUNDUP(temp_stack_ptr,8), entrypoint);
    panic("enter_new_process returned\n");
    return EINVAL;
}

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  #if OPT_A2
       p->p_exitcode =exitcode;
  #else
      (void)exitcode;
  #endif

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
  #if OPT_A2
    *retval = curproc->p_pid;
  #else
    *retval = 1;
  #endif
  return 0;
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
  //exitstatus = 0;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;

  #if OPT_A2
  struct proc *child = NULL;
  for(int i=array_num(curproc->p_children)-1; i >= 0; i--){
			child= array_get(curproc->p_children,i);
      if (child->p_pid == pid){
        break;
      }
  }
  if (child ==NULL){
    return ENOMEM;
  }
  lock_acquire(child->p_lk);
  if(child->p_exitStatus ==-1){
    cv_wait(child->p_cv,child->p_lk);
  }
  lock_release(child->p_lk);
  exitstatus = _MKWAIT_EXIT(child->p_exitcode);
  child_destroy(child);
  #else
  exitstatus = 0;
  #endif

  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return result;
  }
  *retval = pid;

  return 0;
}

void thread_fork_init(void * data1, unsigned long data2){
    (void)data2;
    enter_forked_process((struct trapframe *) data1);

}

pid_t sys__fork(struct trapframe *ctf, pid_t *retval) {
  struct trapframe *tf_child = kmalloc(sizeof(struct trapframe));
  if (tf_child == NULL){
    return ENOMEM;
  }
  *tf_child = *ctf;

  struct proc *childproc = proc_create_runprogram(curproc->p_name);
  struct proc *p = curproc;
  childproc->p_parent = curproc;
  array_add(p->p_children,childproc, NULL);

  if(childproc == NULL){
    kfree(tf_child);
    return ENOMEM;
  } 

  int res = as_copy(p->p_addrspace,&childproc->p_addrspace);
  if(res!=0){
    kfree(tf_child);
    proc_destroy(childproc);
    return ENOMEM;
  }
  pid_t result = thread_fork(p->p_name,childproc,thread_fork_init,tf_child,0);
  if(result != 0){
    as_destroy(childproc->p_addrspace);
    proc_destroy(childproc);
    kfree(tf_child);
    DEBUG(DB_SYSCALL, "thread forking failed");
    return ENOMEM;
  }
  *retval = childproc->p_pid;
  return 0;
}
