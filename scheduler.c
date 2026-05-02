#include <scheduler.h>

// GLOBAL SETUP
// the OS's array of Task Control Blocks
task_control_block_t os_tasks[MAX_TASKS];
// the OS's index tracking which task is running
volatile uint8_t os_current_task = 0;
// keeps count of tasks created in OS
volatile uint8_t os_task_count = 0;
// Max tasks that can be created is 2 and
// each task will get 128 bytes of static RAM.
static uint8_t task_stacks[MAX_TASKS][TASK_STACK_SIZE];

// Initialize the scheduler
void os_init(void)
{
  // Make sure everything starts at zero
  os_current_task = 0;
  os_task_count = 0;
  memeset(os_tasks, 0, sizeof(os_tasks));
}