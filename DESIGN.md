## 1. 项目定位

Smart-OS 是一个轻量级实时内核，以 **EDF（Earliest Deadline First，最早截止时间优先）** 为核心调度策略，面向硬实时场景设计。相比传统优先级调度，EDF 在理论上能提供更高的 CPU 利用率，适合周期性实时任务。

### 设计目标
- **极简内核**：代码量小，便于理解与验证
- **硬实时保证**：支持可调度性分析（利用率 ≤ 100%）
- **模块化设计**：硬件驱动与内核分离，便于移植
- **教学友好**：代码清晰，适合学习实时系统原理

## 2. 核心架构

### 2.1 调度器设计

**EDF 调度算法**：
- 每次调度时，遍历所有 `READY` 状态任务
- 选择 `deadline`（绝对截止时间）最小的任务运行
- 理论上，当任务集利用率 ≤ 100% 时可保证所有任务满足截止时间

**调度触发时机**：
1. 任务主动调用 `smart_task_yield()`
2. SysTick 中断检测到等待任务到达 `arrival` 时间
3. 系统启动时首次调度

### 2.2 任务模型

```c
struct smart_task {
    void *sp;                    // 栈指针（必须在首位，用于上下文切换）
    
    // 任务属性
    void (*entry)(void*);        // 入口函数
    void *parameter;             // 参数
    void *stack_addr;           // 栈起始地址
    uint32_t stack_size;        // 栈大小
    
    // EDF 调度核心参数
    smart_time_t deadline;       // 绝对截止时间
    smart_time_t period;         // 周期（0 表示非周期任务）
    smart_time_t arrival;        // 下一次到达时间
    
    uint8_t state;              // 任务状态
    struct smart_task *next;    // 任务链表指针
};
```

**时间参数说明**：
- `period`: 周期性任务的周期长度（tick 数）
- `relative_deadline`: 相对截止时间，通常等于 `period`
- `deadline`: 绝对截止时间 = `os_tick + relative_deadline`
- `arrival`: 下一次任务到达时间，用于周期性任务唤醒

### 2.3 任务状态机

```
INIT → READY → RUNNING → WAITING → READY (周期性任务)
              ↓
           READY (非周期任务 yield 后)
```

- **INIT**: 初始状态（未使用）
- **READY**: 就绪状态，等待调度器选择
- **RUNNING**: 正在运行（由调度器设置，实际运行在用户态）
- **WAITING**: 等待下一周期到达（周期性任务 yield 后）
- **SUSPEND**: 挂起状态（预留接口）

### 2.4 代码结构

```
Smart-OS/
├── core/
│   ├── smart_core.h      # 内核 API 定义
│   └── smart_core.c      # 调度器、任务管理实现
├── arch/
│   └── context.S         # Cortex-M3 上下文切换汇编
├── drivers/
│   ├── smart_uart.h      # UART 驱动接口
│   └── smart_uart.c      # UART 驱动实现（LM3S 系列）
├── user/
│   └── main.c            # 用户任务示例
├── startup.S             # 系统启动代码
├── link.lds              # 链接脚本
└── Makefile              # 构建脚本
```

## 3. 关键机制

### 3.1 临界区保护

采用嵌套计数机制，支持重入调用：

```c
static volatile uint32_t critical_nesting = 0;

void smart_enter_critical(void) {
    __disable_irq();      // CPSID I
    critical_nesting++;
}

void smart_exit_critical(void) {
    if (critical_nesting > 0) {
        critical_nesting--;
        if (critical_nesting == 0) {
            __enable_irq();  // CPSIE I
        }
    }
}
```

**使用场景**：
- 任务链表操作（创建、删除、遍历）
- 任务状态修改
- 调度器选择过程

### 3.2 中断优先级配置

Cortex-M3 中断优先级寄存器 `SCB_SHPR3`：
- **PendSV (bit 16-23)**: 设为 `0xFF`（最低优先级）
    - 确保上下文切换不会打断其他中断处理
    - 所有中断处理完成后才执行任务切换
- **SysTick (bit 24-31)**: 设为 `0xFE`（次低优先级）
    - 定时器中断，用于系统时钟和任务唤醒

### 3.3 上下文切换机制

**PendSV Handler 流程**：
1. 保存当前任务上下文（R4-R11 到任务栈）
2. 更新 `current_task->sp`
3. 加载下一任务栈指针
4. 恢复下一任务上下文（R4-R11）
5. 更新 `current_task = next_task`
6. 异常返回，硬件自动恢复 R0-R3, R12, LR, PC, xPSR

**启动第一个任务**：
- `start_first_task()` 初始化 PSP，触发 SVC
- `SVC_Handler` 切换到线程模式，使用 PSP，开启中断
- 异常返回后跳转到任务入口函数

### 3.4 硬件抽象层

**UART 驱动**（`drivers/smart_uart.*`）：
- 封装硬件寄存器访问
- 提供统一接口：`smart_uart_init()`, `smart_uart_putc()`, `smart_uart_print()`
- 内核代码不直接访问硬件，便于移植到其他平台

**当前实现**：针对 LM3S811（Cortex-M3），115200 波特率

### 3.5 Idle 任务

系统内置 Idle 任务，当所有用户任务处于 `WAITING` 状态时：
- 调度器选择 Idle 任务运行
- Idle 任务执行 `WFI`（Wait For Interrupt）指令
- 进入低功耗模式，等待中断唤醒

## 4. API 说明

### 4.1 系统初始化

```c
void smart_os_init(void);
void smart_os_start(void);
```

- `smart_os_init()`: 初始化内核数据结构，创建 Idle 任务
- `smart_os_start()`: 配置 SysTick、中断优先级，启动调度器

### 4.2 任务管理

```c
void smart_task_create(smart_task_t task,
                       void (*entry)(void*),
                       void *param,
                       void *stack,
                       uint32_t stack_size,
                       smart_time_t period,
                       smart_time_t relative_deadline);

void smart_task_yield(void);
```

- `smart_task_create()`: 创建任务，初始化栈和 EDF 参数
- `smart_task_yield()`: 任务主动让出 CPU，周期性任务进入 `WAITING` 状态

### 4.3 时间管理

```c
smart_time_t smart_get_tick(void);
```

返回系统时钟 tick 数（1 tick = 1ms，由 SysTick 配置）

### 4.4 临界区

```c
void smart_enter_critical(void);
void smart_exit_critical(void);
```

嵌套安全的临界区保护

## 5. 与 RT-Thread 差异化

| 特性 | Smart-OS | RT-Thread |
|------|----------|-----------|
| 调度策略 | EDF（最早截止时间优先） | 优先级 + 时间片轮转 |
| 代码量 | ~500 行核心代码 | 数万行 |
| 实时性 | 硬实时，支持可调度性分析 | 软实时为主 |
| 任务模型 | 周期性任务，deadline 驱动 | 优先级驱动 |
| 适用场景 | 硬实时系统、教学、验证 | 通用嵌入式系统 |
| 可扩展性 | 模块化，便于添加新调度算法 | 功能完整但复杂 |

**创新点**：
1. **EDF 调度**：理论最优，CPU 利用率高
2. **极简设计**：代码清晰，便于形式化验证
3. **硬件抽象**：驱动与内核分离，移植成本低
