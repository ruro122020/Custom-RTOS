# Custom Mini RTOS

A tiny real-time operating system (RTOS) for the ATmega328P (the chip on the
Arduino Uno), written from scratch in C and AVR assembly.

## Goal

**Learn how an RTOS works internally** — not by reading about one, but by
building one. Off-the-shelf systems like FreeRTOS hide the interesting parts
behind layers of portability code. This project implements the core mechanism
directly, so every step of a context switch is visible and understandable:

- How each task gets its own stack, and how a "fake" stack frame makes a
  brand-new task look like one that was previously interrupted
- How a hardware timer interrupt drives preemptive multitasking
- How the CPU's registers, Status Register (SREG), and Stack Pointer (SP) are
  saved and restored during a context switch
- How `reti` (Return from Interrupt) resumes a task exactly where it left off

## How it works

The scheduler is a **preemptive round-robin** scheduler: every 10ms a timer
interrupt fires, freezes the running task, and resumes the next one. Tasks
never have to cooperate or yield — the hardware timer forces the switch.

The cycle looks like this:

1. A task is running normally.
2. Timer1 reaches its compare value (every 10ms) and fires an interrupt.
3. The CPU pushes the Program Counter (PC) onto the task's stack and jumps
   to the ISR (Interrupt Service Routine).
4. `SAVE_CONTEXT` pushes all 32 registers and SREG, then saves the stack
   pointer into the task's TCB (Task Control Block).
5. The scheduler advances to the next task (round-robin).
6. `RESTORE_CONTEXT` loads the next task's saved stack pointer and pops all
   of its registers back.
7. `reti` jumps to wherever that task was interrupted, and it continues as
   if nothing happened.

## Project layout

| File          | Purpose                                                        |
| ------------- | -------------------------------------------------------------- |
| `scheduler.h` | Public API — data structures and function declarations         |
| `scheduler.c` | Implementation — task creation, timer setup, context switching |

## Public API

```c
void   os_init(void);                        // reset scheduler state
int8_t os_task_create(task_func_t function); // register a task (max 2)
void   os_start(void);                       // start the first task; never returns
```

## Hardware assumptions

- **MCU:** ATmega328P @ 16MHz
- **Timer:** Timer1 in CTC (Clear Timer on Compare) mode, /256 prescaler,
  `OCR1A = 624` → one tick every 10ms
- **Tasks:** up to 2 tasks, each with a 128-byte statically allocated stack
