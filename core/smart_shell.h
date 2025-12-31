#ifndef __SMART_SHELL_H__
#define __SMART_SHELL_H__

#include <stdint.h>

/* Shell 配置 */
#define SHELL_INPUT_BUFFER_SIZE  128
#define SHELL_MAX_ARGS           16
#define SHELL_PROMPT             "SmartOS> "

/* 命令处理函数类型 */
typedef int (*shell_cmd_func_t)(int argc, char *argv[]);

/* Shell 初始化 */
void smart_shell_init(void);

/* Shell 任务入口（内部使用） */
void shell_task_entry(void *param);

#endif
