/*
 * scheduler.h is the Public API for Mini RTOS
 *
 * This file defines the data structures and function signatures that make up
 * the scheduler. Think of it as the "table of contents," the actual
 * implementation lives in scheduler.c.
 *
 * You shouldn't need to change much in this file. It's mostly reading.
 * The real work happens in scheduler.c.
 */
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h> /* gives us uint8_t, uint16_t, etc. — fixed-size integers */

#define MAX_TASKS 2
#define TASK_STACK_SIZE 128
typedef struct
{
  volatile uint8_t *stack_pointer;
} task_control_block_t;

typedef void (*task_func_t)(void);

extern task_control_block_t os_tasks[MAX_TASKS];

extern volatile uint8_t os_current_task;

extern volatile uint8_t os_task_count;

void os_init(void);

int8_t os_task_create(task_func_t function);

void os_start(void);

#endif
