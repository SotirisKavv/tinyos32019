
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "kernel_cc.h"

//1996//
static uint cur_tid = 2; //tid=1 has only the main thread

/*
  Function to start a thread  //1996//
*/
void start_thread()
{
  int exitval;
  PTCB* myptcb = CURTHREAD->owner_ptcb;
  Task call =  myptcb->task;
  exitval = call(myptcb->argl, myptcb->args);

  ThreadExit(exitval);
}

/** 
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{
  PCB* pcb = CURPROC;

  PTCB* ptcb = (PTCB*)malloc(sizeof(PTCB));     //initialization of ptcb
  assert(ptcb);

  ptcb->pcb = pcb;
  ptcb->task = task;
  ptcb->ref_count = 0;
  ptcb->exited = 0;
  ptcb->detached = 0;
  ptcb->tid = cur_tid++;
  ptcb->exit_cv = COND_INIT;

  ptcb->argl = argl;
  if (args != NULL){
    ptcb->args = malloc(argl);
    memcpy(args, ptcb->args, argl);
  } else ptcb->args = NULL;

  //thread spawn and linking
  TCB* tcb = spawn_thread(pcb, ptcb, start_thread);
  ptcb->tcb = tcb; 

  rlist_push_back(& pcb->ptcb_list, rlnode_init(& ptcb->thread_list_node, ptcb));

  wakeup(tcb);

	return (Tid_t) ptcb->tid;
}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf()
{
	return (Tid_t) CURTHREAD->owner_ptcb->tid;
}

/**
  @brief Join the given thread.
  */
int sys_ThreadJoin(Tid_t tid, int* exitval)
{

  if (sys_ThreadSelf() == tid)  return -1;

  PTCB* ptcb = CURTHREAD->owner_ptcb;

  if (ptcb == NULL || ptcb->detached == 1) return -1;

  ptcb->ref_count++;
  while(ptcb->exited != 1 && ptcb->detached != 1)
    kernel_wait(& ptcb->exit_cv, SCHED_USER);

  *exitval = ptcb->exitval;
  ptcb->ref_count--;

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

  if (ptcb->ref_count <= 0){
    rlist_remove(& ptcb->thread_list_node);
    free(ptcb);
  }

  kernel_sleep(EXITED, SCHED_USER);
}
