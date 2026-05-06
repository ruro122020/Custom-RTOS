#include <scheduler.h>
#include <avr/io.h>

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

// Register a task with the scheduler
int8_t os_task_create(task_func_t function)
{
  // check if there is room for another task
  if (os_task_count >= MAX_TASKS)
    return -1;

  // get a pointer to the TOP of the task's stack
  uint8_t *stack_top = &task_stacks[os_task_count][TASK_STACK_SIZE - 1];

  // push the task function's address (the Program Counter (PC))
  uint16_t func_addr = (uint16_t)function;
  *stack_top-- = (uint8_t)(func_addr & 0xFF);      // PC low byte
  *stack_top-- = (uint8_t)(func_addr >> 8 & 0xFF); // PC high byte

  // push 32 general-purpose registers (r0-r31)
  for (uint8_t i = 0; i < 32; i++)
  {
    *stack_top-- = 0x00; // r31, r30, r29, ... r1, r0
  }

  // Enable Status Register (SREG) with interrupts
  *stack_top-- = 0x80;

  // save the resulting stack pointer into the TCB
  os_tasks[os_task_count].stack_pointer = stack_top;

  // increment the task counter
  os_task_count++;
  return 0;
}

// Timer1 setup
static void os_timer_init(void)
{
  // TCCR1A (Timer/Counter1 Control Register A)
  TCCR1A = 0x00;
  /*
   *  Prescaler options (for reference):
   *      CS12:CS11:CS10 = 001 → No prescaler (too fast for our needs)
   *      CS12:CS11:CS10 = 010 → /8
   *      CS12:CS11:CS10 = 011 → /64
   *      CS12:CS11:CS10 = 100 → /256  ← we use this
   *      CS12:CS11:CS10 = 101 → /1024
   *
   *  Breaking down:
   *      WGM12 = 1 → CTC mode (clear timer on compare match with OCR1A)
   *      CS12  = 1, CS11 = 0, CS10 = 0 → Clock / 256 prescaler
   */
  // TCCR1B (Timer/Counter1 Control Register B)
  // Setting WGM12 bit for CTC(Clear Timer on Compare) and CS12 bit for /256 prescaler
  TCCR1B = (1 << WGM12) | (1 << CS12);

  // OCR1A (Output Compare Register 1A)
  // The value the timer counts up to before firing the interrupt
  OCR1A = 624; //(16MHz / 256 / 100Hz) - 1
  // TCNT1 (Timer/Cointer1 value)
  // reset the timer counter to zero so it starts fresh
  TCNT1 = 0;
  // TIMSK1 (Timer/Counter1 Interrupt Mask Register)
  // Enable the Output Compare A Match interrupt
  // This tells the CPU : "When Timer1 reaches OCR1A, jump to the
  // ISR(TIMER1_COMPA_vect) function."
  TIMSK1 |= (1 << OCIE1A);
}