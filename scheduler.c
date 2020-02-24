#define _XOPEN_SOURCE
#define _XOPEN_SOURCE_EXTENDED

#include "scheduler.h"

#include <assert.h>
#include <curses.h>
#include <ucontext.h>
#include <sys/time.h>
#include <math.h>

#include "util.h"

// This is an upper limit on the number of tasks we can create.
#define MAX_TASKS 128

// This is the size of each task's stack memory
#define STACK_SIZE 65536

// This struct will hold the all the necessary information for each task
typedef struct task_info {
  // This field stores all the state required to switch back to this task
  ucontext_t context;
  
  // This field stores another context. This one is only used when the task
  // is exiting.
  ucontext_t exit_context;
  
  // TODO: Add fields here so you can:
  //   a. Keep track of this task's state.
  //   b. If the task is sleeping, when should it wake up?
  //   c. If the task is waiting for another task, which task is it waiting for?
  //   d. Was the task blocked waiting for user input? Once you successfully
  //      read input, you will need to save it here so it can be returned.

  // Time to wake up (ms)
  long wake_up_time;
  // Task to complete before this task
  int waiting_on_task;
  // This task is waiting on input
  bool waiting_on_input;
  // Input saved
  int input;
  // True if task has finished and exit_context ran
  bool done;
} task_info_t;

int current_task; //< The handle of the currently-executing task
int num_tasks = 1;    //< The number of tasks created so far
task_info_t tasks[MAX_TASKS]; //< Information for every task
bool main_task_flag = true;


/**
 * Helper function that checks if a current task is ready to run
 */
bool task_ready(task_info_t task, int task_id){
  if(task.done){
    return false;
  }
  // If task blocked to sleep
  if(task.wake_up_time){
    //printf("Task: %d. Wake up time: %ld\n", task_id,task.wake_up_time);
    // Get current time
    struct timeval start;
    gettimeofday(&start, NULL);
    //printf("Current TIme: %ld\n",((start.tv_sec*1000) + (start.tv_usec/1000)));
    // Check wake up time
    if(task.wake_up_time <= ((start.tv_sec*1000) + (start.tv_usec/1000))){
      //printf("Task: %d now ready to wake up\n", task_id);
      
      return true;
    }
  }
  // If task blocked on task
  if(task.waiting_on_task){
    if(tasks[task.waiting_on_task].done){
      //printf("Task: %d now ready to run because task %d is done\n", task_id, task.waiting_on_task);
      return true;
    }
  }
  // If task is waiting on input
  if(task.waiting_on_input){
    return true;
  }

  // If task is not waiting on anything
  else if(! (task.wake_up_time || task.waiting_on_input || task.waiting_on_task)){
    return true;
  }
  return false;
}



void scheduler(){
  // Loop through tasks
  for(int k = fmod(current_task+1, num_tasks);; k = fmod(k+1,num_tasks)){
    if(task_ready(tasks[k], k)){
      //printf("Swapping context to new task: %d\n", k);
      //printf("Old task: %d\n", current_task);
      int tmp = current_task;
      current_task = k;
      swapcontext(&tasks[tmp].context, &tasks[k].context);
      return;
    }
  }

}


/**
 * This function will execute when a task's function returns. This allows you
 * to update scheduler states and start another task. This function is run
 * because of how the contexts are set up in the task_create function.
 */
void task_exit() {
  // TODO: Handle the end of a task's execution here (Free stack space)
  tasks[current_task].done = true;

  // If main completed
  if(current_task == 0){
    return;
  }
  scheduler();
}




/**
 * Initialize the scheduler. Programs should call this before calling any other
 * functiosn in this file.
 */
void scheduler_init() {
  // TODO: Initialize the state of the scheduler 
  // Claim zero index for the main task
  int index = 0;
  current_task = index;
  
  // Set main task as a struct. Assign context.
  task_info_t main_task;
  getcontext(&main_task.context);

  // Allocate a stack for the main task and add it to the context
  main_task.context.uc_stack.ss_sp = malloc(STACK_SIZE);
  main_task.context.uc_stack.ss_size = STACK_SIZE;

  tasks[index].context = main_task.context;
}





/**
 * Create a new task and add it to the scheduler.
 *
 * \param handle  The handle for this task will be written to this location.
 * \param fn      The new task will run this function.
 */
void task_create(task_t* handle, task_fn_t fn) {
  // Claim an index for the new task
  int index = num_tasks;
  num_tasks++;
  
  // Set the task handle to this index, since task_t is just an int
  *handle = index;
 
  // We're going to make two contexts: one to run the task, and one that runs at the end of the task so we can clean up. Start with the second
  
  // First, duplicate the current context as a starting point
  getcontext(&tasks[index].exit_context);
  
  // Set up a stack for the exit context
  tasks[index].exit_context.uc_stack.ss_sp = malloc(STACK_SIZE);
  tasks[index].exit_context.uc_stack.ss_size = STACK_SIZE;
  
  // Set up a context to run when the task function returns. This should call task_exit.
  makecontext(&tasks[index].exit_context, task_exit, 0);
  
  // Now we start with the task's actual running context
  getcontext(&tasks[index].context);
  
  // Allocate a stack for the new task and add it to the context
  tasks[index].context.uc_stack.ss_sp = malloc(STACK_SIZE);
  tasks[index].context.uc_stack.ss_size = STACK_SIZE;
  
  // Now set the uc_link field, which sets things up so our task will go to the exit context when the task function finishes
  tasks[index].context.uc_link = &tasks[index].exit_context;
  
  // And finally, set up the context to execute the task function
  makecontext(&tasks[index].context, fn, 0);

  tasks[index].done = false;

  printf("Created Task: %d\n", index);
}


/**
 * Wait for a task to finish. If the task has not yet finished, the scheduler should
 * suspend this task and wake it up later when the task specified by handle has exited.
 *
 * \param handle  This is the handle produced by task_create
 */
void task_wait(task_t handle) {
  // TODO: Block this task until the specified task has exited.

  printf("Set wait on task %d for task %d\n", current_task, handle);

  // Set task that this task is waiting on
  tasks[current_task].waiting_on_task = handle;

  //Call scheduler
  scheduler();
}


/**
 * The currently-executing task should sleep for a specified time. If that time is larger
 * than zero, the scheduler should suspend this task and run a different task until at least
 * ms milliseconds have elapsed.
 * 
 * \param ms  The number of milliseconds the task should sleep.
 */
void task_sleep(size_t ms) {
  // TODO: Block this task until the requested time has elapsed.
  // Hint: Record the time the task should wake up instead of the time left for it to sleep. The bookkeeping is easier this way.
  printf("Set sleep time.\n");

  // Get current time
  struct timeval start;
  gettimeofday(&start, NULL);

  // Set wake up time
  tasks[current_task].wake_up_time = (long)(start.tv_sec*1000) + (long)(start.tv_usec/1000) + (long)ms;

  // Call scheduler
  scheduler();
}


/**
 * Read a character from user input. If no input is available, the task should
 * block until input becomes available. The scheduler should run a different
 * task while this task is blocked.
 *
 * \returns The read character code
 */
int task_readchar() {
  // TODO: Block this task until there is input available.
  // To check for input, call getch(). If it returns ERR, no input was available.
  // Otherwise, getch() will returns the character code that was read.

  int c = getch();
  printf("%c\n", c);
  // If no input 
  if(c == ERR){
    // Set that this task is waiting on INPUT
    tasks[current_task].waiting_on_input = true;
  }
  else{
    tasks[current_task].waiting_on_input = false;
    tasks[current_task].input = c;
  }


  return c;
}
