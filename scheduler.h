#include <stdint.h> /* gives us uint8_t, uint16_t, etc. — fixed-size integers */

#define MAX_TASKS 2
#define TASK_STACK_SIZE 128
typedef struct
{
  volatile uint8_t *stack_pointer;
} task_control_block_t;

typedef void (*task_func_t)(void);