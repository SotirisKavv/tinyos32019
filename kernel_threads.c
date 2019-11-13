
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "kernel_cc.h"

/*
  Function to start a thread  //1996//
*/
void start_thread()
{
  int exitval;
  PTCB* ptcb = CURTHREAD->owner_ptcb;
  Task call =  ptcb->task;
  exitval = call(ptcb->argl, ptcb->args);

  sys_ThreadExit(exitval);
}

/** 
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{
  PCB* pcb = CURPROC;

  PTCB* ptcb = (PTCB*)malloc(sizeof(PTCB));     //initialization of ptcb
  assert(ptcb);

  CURPROC->thread_count++;

  ptcb->pcb = pcb;
  ptcb->task = task;
  ptcb->ref_count = 0;
  ptcb->exited = 0;
  ptcb->detached = 0;
  ptcb->exitval = 0;
  ptcb->exit_cv = COND_INIT;

  ptcb->argl = argl;
  ptcb->args = args;

  rlist_push_back(& pcb->ptcb_list, rlnode_init(& ptcb->thread_list_node, ptcb));
  
  if (task != NULL)
  {
    TCB* tcb = spawn_thread(pcb, ptcb, start_thread);
    wakeup(tcb);
  }

	return (Tid_t) ptcb;
}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf()
{
	return (Tid_t) CURTHREAD->owner_ptcb;
}

/**
  @brief Join the given thread.
  */
int sys_ThreadJoin(Tid_t tid, int* exitval)
{
  PTCB* ptcb = (PTCB*) tid;

  if(rlist_find(& CURPROC->ptcb_list, ptcb, NULL) == NULL)   return -1;

  if (ptcb == NULL || ptcb->detached == 1 || ptcb->exited == 1) return -1;

  ptcb->ref_count++;

  while(ptcb->exited != 1 && ptcb->detached != 1)
    kernel_wait(& ptcb->exit_cv, SCHED_USER);
  
  ptcb->ref_count--;
  
  if (ptcb->exited == 1)
  {
    ptcb->ref_count = 0;

    if (exitval!=NULL)
    {
      *exitval = ptcb->exitval;
    }
  } 
	return 0;
}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{
  PTCB* ptcb = (PTCB*) tid;

  if (ptcb == NULL || ptcb->exited == 1) return -1;

  ptcb->detached = 1;
  kernel_broadcast(& ptcb->exit_cv);
  ptcb->ref_count = 0;

  return 0;
}

/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{
  PTCB* ptcb = CURTHREAD->owner_ptcb;

  ptcb->exited = 1;
  ptcb->exitval = exitval;

  kernel_broadcast(& ptcb->exit_cv);

  if (ptcb->pcb->thread_count == 0)
  {
    sys_Exit(exitval);
  }

  ptcb->pcb->thread_count--;

  rlist_remove(& ptcb->thread_list_node);
  free(ptcb);

  kernel_sleep(EXITED, SCHED_USER);
}
