#ifndef __SNAKE_GAME_H__
#define __SNAKE_GAME_H__

#include <stdint.h>

/* 游戏配置 */
#define SNAKE_WIDTH     20
#define SNAKE_HEIGHT    10
#define SNAKE_MAX_LEN   (SNAKE_WIDTH * SNAKE_HEIGHT)

/* 方向 */
typedef enum {
    DIR_UP = 0,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT
} snake_direction_t;

/* 游戏状态 */
typedef enum {
    GAME_RUNNING = 0,
    GAME_OVER,
    GAME_WIN,
    GAME_PAUSED
} game_state_t;

/* 位置 */
typedef struct {
    int8_t x;
    int8_t y;
} position_t;

/* 蛇 */
typedef struct {
    position_t body[SNAKE_MAX_LEN];
    uint16_t length;
    snake_direction_t direction;
} snake_t;

/* 游戏上下文 */
typedef struct {
    snake_t snake;
    position_t food;
    game_state_t state;
    uint32_t score;
    uint32_t high_score;
} game_context_t;

/* 游戏API */
void snake_game_init(void);
void snake_game_start(void);
void snake_game_update(void);
void snake_game_render(void);
void snake_game_input(char key);
void snake_game_exit(void);
game_state_t snake_game_get_state(void);

#endif
