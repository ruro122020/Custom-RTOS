/*
 * scheduler.c is the implementation of the Mini RTOS scheduler
 *
 * This file contains the actual working code behind the API declared in
 * scheduler.h. While the header is the "table of contents," this is the
 * book itself: task creation, the timer setup, the context-switch
 * machinery, and the scheduler startup all live here.
 *
 * What's inside:
 *   - os_tasks[] / os_current_task / os_task_count — the scheduler's state
 *   - os_init() — reset the scheduler state
 *   - os_task_create() — build a fake stack frame so a task can be "resumed"
 *   - os_timer_init() — configure Timer1 to fire every 10ms
 *   - SAVE_CONTEXT / RESTORE_CONTEXT — push/pop all registers during a switch
 *   - ISR(TIMER1_COMPA_vect) — the interrupt that performs the task switch
 *   - os_start() — kick off the first task and hand control to the timer
 */

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
  memset(os_tasks, 0, sizeof(os_tasks));
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

/**
 * The math to context switch every 10ms at 16MHz with a prescaler of 256:
 *
 * - We get the 16MHz from the F_CPU(CPU Clock Frequency)in the ATmega328P chip we are using.
 * - The prescaler divides the CPU clock down to a slower timer clock. The available prescaler values are:
 *   1, 8, 64, 256, 1024, etc. which are defined by the chip's timer peripheral in the datasheet's timer registers description.
 * With the MHz and prescaler values we calculate the desired tick rate to context switch.
 * I chose to start with 10ms. So the calculation will be:
 *   Timer frequency = CPU_clock / prescaler = 16,000,000 / 256 = 62,500 Hz
 *   Ticks per 10ms  = 62,500 * 0.010 = 625
 *   So OCR1A = 625 - 1 = 624
 */

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

#define SAVE_CONTEXT()                                \
  asm volatile(                                       \
      "push r0 \n\t"                                  \
      "in r0, __SREG__ \n\t" /* read SREG into r0 */  \
      "cli \n\t"             /* disable interrupts */ \
      "push r0 \n\t"         /* push SREG value */    \
      "push r1 \n\t"                                  \
      "clr  r1 \n\t" /* GCC expects r1=0 */           \
      "push r2 \n\t"                                  \
      "push r3 \n\t"                                  \
      "push r4 \n\t"                                  \
      "push r5 \n\t"                                  \
      "push r6 \n\t"                                  \
      "push r7 \n\t"                                  \
      "push r8 \n\t"                                  \
      "push r9 \n\t"                                  \
      "push r10 \n\t"                                 \
      "push r11 \n\t"                                 \
      "push r12 \n\t"                                 \
      "push r13 \n\t"                                 \
      "push r14 \n\t"                                 \
      "push r15 \n\t"                                 \
      "push r16 \n\t"                                 \
      "push r17 \n\t"                                 \
      "push r18 \n\t"                                 \
      "push r19 \n\t"                                 \
      "push r20 \n\t"                                 \
      "push r21 \n\t"                                 \
      "push r22 \n\t"                                 \
      "push r23 \n\t"                                 \
      "push r24 \n\t"                                 \
      "push r25 \n\t"                                 \
      "push r26 \n\t"                                 \
      "push r27 \n\t"                                 \
      "push r28 \n\t"                                 \
      "push r29 \n\t"                                 \
      "push r30 \n\t"                                 \
      "push r31 \n\t");                               \
  *os_tasks[os_current_task].stack_pointer = (volatile uint8_t *)SP;

#define RESTORE_CONTEXT()                                            \
  /* Load the new task's saved stack pointer */                      \
  SP = (uint16_t)os_tasks[os_current_task].stack_pointer;            \
  asm volatile(                                                      \
      "pop  r31                          \n\t"                       \
      "pop  r30                          \n\t"                       \
      "pop  r29                          \n\t"                       \
      "pop  r28                          \n\t"                       \
      "pop  r27                          \n\t"                       \
      "pop  r26                          \n\t"                       \
      "pop  r25                          \n\t"                       \
      "pop  r24                          \n\t"                       \
      "pop  r23                          \n\t"                       \
      "pop  r22                          \n\t"                       \
      "pop  r21                          \n\t"                       \
      "pop  r20                          \n\t"                       \
      "pop  r19                          \n\t"                       \
      "pop  r18                          \n\t"                       \
      "pop  r17                          \n\t"                       \
      "pop  r16                          \n\t"                       \
      "pop  r15                          \n\t"                       \
      "pop  r14                          \n\t"                       \
      "pop  r13                          \n\t"                       \
      "pop  r12                          \n\t"                       \
      "pop  r11                          \n\t"                       \
      "pop  r10                          \n\t"                       \
      "pop  r9                           \n\t"                       \
      "pop  r8                           \n\t"                       \
      "pop  r7                           \n\t"                       \
      "pop  r6                           \n\t"                       \
      "pop  r5                           \n\t"                       \
      "pop  r4                           \n\t"                       \
      "pop  r3                           \n\t"                       \
      "pop  r2                           \n\t"                       \
      "pop  r1                           \n\t"                       \
      "pop  r0                           \n\t" /* this is SREG */    \
      "out  __SREG__, r0                 \n\t" /* restore SREG */    \
      "pop  r0                           \n\t" /* restore real r0 */ \
  );

// Implement the Timer ISR
ISR(TIMER1_COMPA_vect, ISR_NAKED)
{
  // save the current task's context
  SAVE_CONTEXT();
  // pick the next task
  os_current_task++;
  if (os_current_task >= os_task_count)
  {
    os_current_task = 0;
  }
  // restore the next task's context
  RESTORE_CONTEXT();
  // return from interrupt into the restored task
  asm volatile("reti");
};

/*
 *   1. Task 1 is happily running, toggling its LED...
 *   2. *BOOM* Timer1 interrupt fires!
 *   3. The CPU automatically pushes the Program Counter (PC) onto
 *      Task 1's stack and jumps to our ISR.
 *   4. SAVE_CONTEXT pushes all of Task 1's registers and saves its SP.
 *   5. We advance os_current_task to point to Task 2.
 *   6. RESTORE_CONTEXT loads Task 2's SP and pops all its registers.
 *   7. reti pops the return address (which is wherever Task 2 was
 *      interrupted last time) and jumps there.
 *   8. Task 2 continues running from exactly where it left off.
 *   9. 10ms later, the timer fires again and the cycle repeats.
 */

void os_start(void)
{
  // start the schedular

  // disable interrupts while we set things up.
  cli();
  /*
   *   cli() sets the I-flag in SREG to 0, preventing any interrupt
   *   from firing while we're configuring the timer and loading
   *   the first task. Without this, a timer interrupt could fire
   *   before we're ready, causing a crash.
   */

  // initialize the timer
  os_timer_init();
  // set the current task to task 0 (the first one created).
  os_current_task = 0;
  // restore task 0's context and start running it
  RESTORE_CONTEXT();

  /*
   *   This loads Task 0's saved stack pointer into the CPU's SP,
   *   then pops all the register values we set up in os_task_create().
   *   The stack now has only the return address (the task function's
   *   address) left on it.
   */

  asm volatile("reti");
  /*
   *   reti pops the return address (our task function) off the stack
   *   and jumps to it. It also re-enables interrupts (sets I-flag).
   *   From this point on, Task 0 is running and the timer interrupt
   *   will take care of switching between tasks.
   */
};