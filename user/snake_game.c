#include "snake_game.h"
#include "smart_uart.h"
#include "smart_core.h"
#include <string.h>

static game_context_t game;

/* ANSI转义序列 */
#define ANSI_CLEAR      "\033[2J"
#define ANSI_HOME       "\033[H"
#define ANSI_HIDE_CURSOR "\033[?25l"
#define ANSI_SHOW_CURSOR "\033[?25h"

/* 随机数生成（简单LCG） */
static uint32_t rand_seed = 12345;

static uint32_t simple_rand(void)
{
    rand_seed = rand_seed * 1103515245 + 12345;
    return (rand_seed / 65536) % 32768;
}

/* 生成食物 */
static void spawn_food(void)
{
    int attempts = 0;
    int valid = 0;
    
    while (!valid && attempts < 100)
    {
        game.food.x = simple_rand() % SNAKE_WIDTH;
        game.food.y = simple_rand() % SNAKE_HEIGHT;
        
        /* 检查是否与蛇身重叠 */
        valid = 1;
        for (uint16_t i = 0; i < game.snake.length; i++)
        {
            if (game.snake.body[i].x == game.food.x && 
                game.snake.body[i].y == game.food.y)
            {
                valid = 0;
                break;
            }
        }
        attempts++;
    }
}

/* 初始化游戏 */
void snake_game_init(void)
{
    memset(&game, 0, sizeof(game));
    
    /* 初始化蛇（中间位置，长度3） */
    game.snake.length = 3;
    game.snake.direction = DIR_RIGHT;
    game.snake.body[0].x = SNAKE_WIDTH / 2;
    game.snake.body[0].y = SNAKE_HEIGHT / 2;
    game.snake.body[1].x = game.snake.body[0].x - 1;
    game.snake.body[1].y = game.snake.body[0].y;
    game.snake.body[2].x = game.snake.body[0].x - 2;
    game.snake.body[2].y = game.snake.body[0].y;
    
    /* 生成食物 */
    rand_seed = smart_get_tick();
    spawn_food();
    
    game.state = GAME_RUNNING;
    game.score = 0;
}

/* 开始游戏 */
void snake_game_start(void)
{
    /* 隐藏光标 */
    smart_uart_print(ANSI_HIDE_CURSOR);
    
    /* 清屏 */
    smart_uart_print(ANSI_CLEAR);
    smart_uart_print(ANSI_HOME);
    
    /* 显示欢迎信息 */
    smart_uart_print("========================================\n");
    smart_uart_print("         SNAKE GAME - Smart-OS\n");
    smart_uart_print("========================================\n");
    smart_uart_print("\n");
    smart_uart_print("Controls:\n");
    smart_uart_print("  W - Up\n");
    smart_uart_print("  S - Down\n");
    smart_uart_print("  A - Left\n");
    smart_uart_print("  D - Right\n");
    smart_uart_print("  P - Pause\n");
    smart_uart_print("  Q - Quit\n");
    smart_uart_print("\n");
    smart_uart_print("Press any key to start...\n");
}

/* 更新游戏逻辑 */
void snake_game_update(void)
{
    if (game.state != GAME_RUNNING)
    {
        return;
    }
    
    /* 计算新头部位置 */
    position_t new_head = game.snake.body[0];
    
    switch (game.snake.direction)
    {
        case DIR_UP:    new_head.y--; break;
        case DIR_DOWN:  new_head.y++; break;
        case DIR_LEFT:  new_head.x--; break;
        case DIR_RIGHT: new_head.x++; break;
    }
    
    /* 检查墙壁碰撞 */
    if (new_head.x < 0 || new_head.x >= SNAKE_WIDTH ||
        new_head.y < 0 || new_head.y >= SNAKE_HEIGHT)
    {
        game.state = GAME_OVER;
        return;
    }
    
    /* 检查自身碰撞 */
    for (uint16_t i = 0; i < game.snake.length; i++)
    {
        if (game.snake.body[i].x == new_head.x && 
            game.snake.body[i].y == new_head.y)
        {
            game.state = GAME_OVER;
            return;
        }
    }
    
    /* 检查是否吃到食物 */
    int ate_food = (new_head.x == game.food.x && new_head.y == game.food.y);
    
    if (ate_food)
    {
        game.score += 10;
        
        /* 增加长度 */
        if (game.snake.length < SNAKE_MAX_LEN)
        {
            game.snake.length++;
        }
        
        /* 生成新食物 */
        spawn_food();
        
        /* 检查胜利条件 */
        if (game.snake.length >= SNAKE_MAX_LEN)
        {
            game.state = GAME_WIN;
            return;
        }
    }
    
    /* 移动蛇身（从尾到头） */
    for (int16_t i = game.snake.length - 1; i > 0; i--)
    {
        game.snake.body[i] = game.snake.body[i - 1];
    }
    
    /* 更新头部 */
    game.snake.body[0] = new_head;
}

/* 渲染游戏画面 */
void snake_game_render(void)
{
    /* 分隔线 */
    smart_uart_print("\n========================================\n");
    
    /* 显示分数 */
    smart_uart_print("Score: ");
    smart_uart_print_hex32(game.score);
    smart_uart_print("  Length: ");
    smart_uart_print_hex32(game.snake.length);
    smart_uart_print("\n\n");
    
    /* 绘制上边框 */
    smart_uart_putc('+');
    for (int x = 0; x < SNAKE_WIDTH; x++)
    {
        smart_uart_putc('-');
    }
    smart_uart_print("+\n");
    
    /* 绘制游戏区域 */
    for (int y = 0; y < SNAKE_HEIGHT; y++)
    {
        smart_uart_putc('|');
        
        for (int x = 0; x < SNAKE_WIDTH; x++)
        {
            char ch = ' ';
            
            /* 检查是否是蛇头 */
            if (game.snake.body[0].x == x && game.snake.body[0].y == y)
            {
                ch = '@';
            }
            /* 检查是否是蛇身 */
            else
            {
                for (uint16_t i = 1; i < game.snake.length; i++)
                {
                    if (game.snake.body[i].x == x && game.snake.body[i].y == y)
                    {
                        ch = 'o';
                        break;
                    }
                }
            }
            
            /* 检查是否是食物 */
            if (game.food.x == x && game.food.y == y)
            {
                ch = '*';
            }
            
            smart_uart_putc(ch);
        }
        
        smart_uart_print("|\n");
    }
    
    /* 绘制下边框 */
    smart_uart_putc('+');
    for (int x = 0; x < SNAKE_WIDTH; x++)
    {
        smart_uart_putc('-');
    }
    smart_uart_print("+\n");
    
    /* 显示状态 */
    if (game.state == GAME_OVER)
    {
        smart_uart_print("\n*** GAME OVER ***\n");
        smart_uart_print("Press R to restart, Q to quit\n");
    }
    else if (game.state == GAME_WIN)
    {
        smart_uart_print("\n*** YOU WIN! ***\n");
        smart_uart_print("Press R to restart, Q to quit\n");
    }
    else if (game.state == GAME_PAUSED)
    {
        smart_uart_print("\n*** PAUSED ***\n");
        smart_uart_print("Press P to resume\n");
    }
    else
    {
        smart_uart_print("\nW/A/S/D: Move  P: Pause  Q: Quit\n");
    }
}

/* 处理输入 */
void snake_game_input(char key)
{
    /* 转换为大写 */
    if (key >= 'a' && key <= 'z')
    {
        key = key - 'a' + 'A';
    }
    
    if (game.state == GAME_RUNNING)
    {
        switch (key)
        {
            case 'W':
                if (game.snake.direction != DIR_DOWN)
                {
                    game.snake.direction = DIR_UP;
                }
                break;
                
            case 'S':
                if (game.snake.direction != DIR_UP)
                {
                    game.snake.direction = DIR_DOWN;
                }
                break;
                
            case 'A':
                if (game.snake.direction != DIR_RIGHT)
                {
                    game.snake.direction = DIR_LEFT;
                }
                break;
                
            case 'D':
                if (game.snake.direction != DIR_LEFT)
                {
                    game.snake.direction = DIR_RIGHT;
                }
                break;
                
            case 'P':
                game.state = GAME_PAUSED;
                break;
        }
    }
    else if (game.state == GAME_PAUSED)
    {
        if (key == 'P')
        {
            game.state = GAME_RUNNING;
        }
    }
    else if (game.state == GAME_OVER || game.state == GAME_WIN)
    {
        if (key == 'R')
        {
            snake_game_init();
        }
    }
}

/* 退出游戏 */
void snake_game_exit(void)
{
    /* 显示光标 */
    smart_uart_print(ANSI_SHOW_CURSOR);
    
    /* 清屏 */
    smart_uart_print(ANSI_CLEAR);
    smart_uart_print(ANSI_HOME);
    
    smart_uart_print("Thanks for playing!\n\n");
}

/* 获取游戏状态 */
game_state_t snake_game_get_state(void)
{
    return game.state;
}
