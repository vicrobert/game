/*
 * Snake v1.0 for Funny
 * Description: this game must be run in linux 
 * Compile tools chain: gcc & gdb
 * Author: Junbo Yang
 * Date: 6/20/2016
 */
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#define WIDTH     30
#define HEIGHT    30
#define ORG_X     1
#define ORG_Y     1
#define DIRECT_UP       1
#define DIRECT_DOWN     2
#define DIRECT_RIGHT    3
#define DIRECT_LEFT     4
#define GAME_STAT_STOP          0
#define GAME_STAT_READY         1
#define GAME_STAT_PLAYING       2
#define GAME_STAT_PAUSE         3
/* 0 < GAME_SPEED_ACC_FACT <= 10.0 */
#define GAME_SPEED_ACC_FACT     10.0

struct body_slice_type
{
    int x;
    int y;
    int direct;
    struct body_slice_type *prev_slice_ptr;
    struct body_slice_type *next_slice_ptr;
};

struct snake_type
{
    int length;
    int speed_factor;
    struct body_slice_type *head;
    struct body_slice_type *tail;
};

#define print_s(s, x, y) \
    printf("\033[%d;%dH%s\n", (y), ((x) << 1), (s))

#define print_i(i, x, y) \
    printf("\033[%d;%dH%d\n", (y), (x), (i))

static struct termios ori_attr, cur_attr;
static struct body_slice_type body_slice_pool[(HEIGHT-2)*(WIDTH-2)];
static struct snake_type snake = {0};
static char stage_map[HEIGHT][WIDTH] = {0};
static int allc_idx = 0;
static int nonblock = 0;
unsigned int eggs = 0;
int total_score = 0;
int game_stat = 0;

int reset_stdin()
{
    return tcsetattr(STDIN_FILENO, TCSANOW, &ori_attr);
}

int set_stdin_nonblock()
{   
    if(nonblock || tcgetattr(STDIN_FILENO, &ori_attr))
        return -1;

    memcpy(&cur_attr, &ori_attr, sizeof(cur_attr));
    cur_attr.c_lflag &= ~(ICANON | ECHO);
    cur_attr.c_cc[VMIN] = 0;
    cur_attr.c_cc[VTIME] = 0;
    nonblock = 1;

    return tcsetattr(STDIN_FILENO, TCSANOW, &cur_attr);
}

int kbhit()
{
    char key[3];
    unsigned int nread;
    
    set_stdin_nonblock();
    
    nread = read(STDIN_FILENO, &key, 3);
    switch(nread) {
    case 1:
        return key[0];
    case 3:
        if (key[2] == 'A') /* UP */
            return 0xe048;
        else if (key[2] == 'B') /* DOWN */
            return 0xe050;
        else if (key[2] == 'C') /* RIGHT */
            return 0xe04d;
        else if (key[2] == 'D') /* LEFT */
            return 0xe04b;
    default:
        return 0;
    }
}

void reset_kb()
{
    if (!reset_stdin())
        nonblock = 0;
}

void draw_body_slice(int x, int y)
{
    print_s("S", x, y);
}

void draw_egg(int x, int y)
{
    print_s("O", x, y);
}

void clear_body_slice(int x, int y)
{
    int i = y - ORG_Y;
    int j = x - ORG_X;
    
    if (stage_map[i][j] == 0)
        print_s(" ", x, y);
    else if (stage_map[i][j] == 1)
        print_s("O", x, y);
    /* todo: someother conditions else */
}

void clear_egg(int x, int y)
{
    print_s(" ", x, y);
}

/*
 * parameter count is ignored at version 1.0
 */
int put_egg(int count)
{
    int i, j, x, x2, y, y2, pos, fail = 0;
    static int failure_times = 0;
    struct body_slice_type *pslice;

    srand(time(0));
    pos = (HEIGHT - 2) * (WIDTH - 2) * (rand() / (RAND_MAX + 1.0));
    
    i = (pos / (WIDTH - 2)) + 1;
    j = (pos % (WIDTH - 2)) + 1;
    x = j + ORG_X;
    y = i + ORG_Y;

    if (stage_map[i][j] != 0)
        fail = 1;
    else {
        pslice = snake.head;
        while (pslice) {
            if (pslice->x == x && pslice->y == y) {
                fail = 1;
                break;
            }
            pslice = pslice->next_slice_ptr;
        }
    }

    if (fail) {
        if ((++ failure_times) > 5) {
            for (y2 = snake.head->y - 1, i = y2 - ORG_Y;
                y2 <= snake.head->y + 1; ++y2, ++i)
                for (x2 = snake.head->x - 1, j = x2 - ORG_X;
                    x2 <= snake.head->x + 1; ++x2, ++j) {
                    if ((y2 > ORG_Y && y2 < ORG_Y + HEIGHT - 1)
                        && (x2 > ORG_X && x2 < ORG_X + WIDTH - 1)
                        && (x2 != y2) && (stage_map[i][j] == 0)) {
                        stage_map[i][j] = 1;
                        failure_times = 0;
                        eggs ++;
                        /* draw egg*/
                        draw_egg(x2, y2);
                        return 0;
                    }
                }
        }
        return -1;
    } else {
        stage_map[i][j] = 1;
        failure_times = 0;
        eggs ++;
        /* draw egg */
        draw_egg(x, y);
        return 0;
    }
}

int score(int clean)
{
    char score_s[20];
    /* calculate */
    if (clean)
        total_score = 0;
    else
        total_score += snake.speed_factor + 1;
    /* show */
    sprintf(score_s, "%d", total_score);
    print_s("                    ", ORG_Y + 6, ORG_Y + HEIGHT);
    print_s(score_s, ORG_Y + 6, ORG_Y + HEIGHT);

    return total_score;
}

int snake_hit_test()
{
    int i, j;

    if (!snake.head || !snake.tail)
        return -1;

    j = snake.head->x - ORG_X;
    i = snake.head->y - ORG_Y;
    if(stage_map[i][j] == 2)
        return 1;
    
    struct body_slice_type *pslice = snake.head->next_slice_ptr;
    while(pslice) {
        if (snake.head->x == pslice->x && snake.head->y == pslice->y)
            return 1;
        pslice = pslice->next_slice_ptr;
    }

    return 0;
}

void update_speed_factor()
{
    double range = (WIDTH - 2) * (HEIGHT - 2) / GAME_SPEED_ACC_FACT;
    int len = (int)(snake.length > range? range: snake.length);
    snake.speed_factor = (int)(10 * len / range);
}

struct body_slice_type * new_body()
{
    static int allc_maximum = (HEIGHT - 2) * (WIDTH - 2);
    assert (allc_idx < allc_maximum);
    return &body_slice_pool[allc_idx ++];
}

int snake_born(int x, int y, int direct)
{
    struct body_slice_type *new_head, *new_tail;
    if (!snake.head && !snake.tail) {
        new_head = new_body();
        if (new_head) {
            new_head->x = x;
            new_head->y = y;
            new_head->direct = direct;
            snake.head = new_head;
        }

        /* tail */
        new_tail = new_body();
        new_tail->direct = direct;
        if(direct == DIRECT_UP) {
            new_tail->x = x;
            new_tail->y = y + 1;
        } else if (direct == DIRECT_DOWN) {
            new_tail->x = x;
            new_tail->y = y - 1;
        } else if (direct == DIRECT_RIGHT) {
            new_tail->x = x - 1;
            new_tail->y = y;                
        } else {
            new_tail->x = x + 1;
            new_tail->y = y;
        }
        snake.tail = new_tail;
        new_head->prev_slice_ptr = NULL;
        new_head->next_slice_ptr = new_tail;
        new_tail->prev_slice_ptr = new_head;
        new_tail->next_slice_ptr = NULL;
        snake.length = 2;
        update_speed_factor();

        /* draw */
        draw_body_slice(new_head->x, new_head->y);
        draw_body_slice(new_tail->x, new_tail->y);

        return 0;
    }
    return -1;
}

int prepares_to_grow()
{
    if (!snake.head || !snake.tail)
        return -1;

    struct body_slice_type *new_tail = new_body();
    
    if (new_tail) {
        /* keep the same direct */
        new_tail->direct = snake.tail->direct;
        /* find position of the new tail */
        if (snake.tail->direct == DIRECT_UP) {
            new_tail->x = snake.tail->x;
            new_tail->y = snake.tail->y + 1;
        } else if (snake.tail->direct == DIRECT_DOWN) {
            new_tail->x = snake.tail->x;
            new_tail->y = snake.tail->y - 1;
        } else if (snake.tail->direct == DIRECT_RIGHT) {
            new_tail->x = snake.tail->x - 1;
            new_tail->y = snake.tail->y;
        } else { /*DIRECT_LEFT*/
            new_tail->x = snake.tail->x + 1;
            new_tail->y = snake.tail->y;
        }

        snake.tail->next_slice_ptr = new_tail;
        new_tail->prev_slice_ptr = snake.tail;
        new_tail->next_slice_ptr = NULL;
        snake.tail = new_tail;
        snake.length ++;
        update_speed_factor();

        return 0;
    }

    return -1;
}

int eat_egg(int x, int y)
{
    int i = y - ORG_Y;
    int j = x - ORG_X;

    if (stage_map[i][j] != 1)
        return -1;

    stage_map[i][j] = 0;
    eggs --;
    clear_egg(x, y);

    /* calculate and show the score */
    score(0);

    return prepares_to_grow();
}

int crawl()
{
    if (!snake.head || !snake.tail)
        return -1;

    int retval;
    static int ready_to_grow = 0;
    struct body_slice_type *pslice_tail = snake.tail;
    struct body_slice_type *pslice = pslice_tail->prev_slice_ptr;
    
    /* clear tail when necessary */
    if (!ready_to_grow)
        clear_body_slice(pslice_tail->x, pslice_tail->y);

    while(pslice) {
        pslice_tail->x = pslice->x;
        pslice_tail->y = pslice->y;
        pslice_tail->direct = pslice->direct;
        pslice_tail = pslice;
        pslice = pslice->prev_slice_ptr;
    }

    if (snake.head->direct == DIRECT_UP)
        snake.head->y = snake.head->y - 1;
    else if (snake.head->direct == DIRECT_DOWN)
        snake.head->y = snake.head->y + 1;
    else if (snake.head->direct == DIRECT_RIGHT)
        snake.head->x = snake.head->x + 1;
    else /* DIRECT_LEFT */
        snake.head->x = snake.head->x - 1;

    /* there is an egg */
    if (stage_map[snake.head->y - ORG_Y][snake.head->x - ORG_X] == 1) {
        eat_egg(snake.head->x, snake.head->y);
        ready_to_grow = 1;
    } else
        ready_to_grow = 0;

    retval = snake_hit_test();
    /* draw head */
    if(!retval)
        draw_body_slice(snake.head->x, snake.head->y);

    return retval;
}

void change_crawling_direct(int direct)
{
    if (!snake.head)
        return;

    if ((snake.head->direct == DIRECT_UP && direct == DIRECT_DOWN)
        || (snake.head->direct == DIRECT_DOWN && direct == DIRECT_UP)
        || (snake.head->direct == DIRECT_LEFT && direct == DIRECT_RIGHT)
        || (snake.head->direct == DIRECT_RIGHT && direct == DIRECT_LEFT))
        return;

    snake.head->direct = direct;
}

void snake_remove(int screen_clear)
{
    struct body_slice_type *pslice = snake.head;
    struct body_slice_type *pslice_next;
    /* clear screen */
    if (screen_clear) {
        while(pslice) {
            pslice_next = pslice->next_slice_ptr;
            clear_body_slice(pslice->x, pslice->y);
            pslice = pslice_next;
        }
    }
    snake.head = NULL;
    snake.tail = NULL;
    snake.length = 0;
    allc_idx = 0;
}

void clear_stage()
{
    int i, j;
    for (i = 0; i < HEIGHT; ++ i)
        for (j = 0; j < WIDTH; ++ j)
            stage_map[i][j] = 0;
    system("clear");
}

void draw_stage()
{
    int i, j;
    
    clear_stage();
    /* drawing vertical line */
    for (i = ORG_Y + 1, j = 1; i < ORG_Y + HEIGHT - 1; ++ i, ++ j) {
        stage_map[j][0] = 2;
        stage_map[j][WIDTH - 1] = 2;
        print_s("#", ORG_X, i);
        print_s("#", ORG_X + WIDTH - 1, i);
    }
    /* drawing horizontal line */
    for (i = ORG_X, j = 0; i < ORG_X + WIDTH; ++ i, ++ j) {
        stage_map[0][j] = 2;
        stage_map[HEIGHT - 1][j] = 2;
        print_s("#", i, ORG_Y);
        print_s("#", i, ORG_Y + HEIGHT - 1);
    }
    /* show score label */
    print_s("SCORE:", ORG_X, ORG_Y + HEIGHT);
}

int timeline_forward()
{
    int key_code;

    key_code = kbhit();
    /* deal game pause */
    if (key_code == ' ' && game_stat == GAME_STAT_PLAYING)
        game_stat = GAME_STAT_PAUSE;
    else if (key_code == ' ' && game_stat == GAME_STAT_PAUSE)
        game_stat = GAME_STAT_PLAYING;

    if (game_stat == GAME_STAT_PAUSE)
        return 0;

    switch(key_code) {
        case 'w':
        case 'W':
        case 0xe048:
            change_crawling_direct(DIRECT_UP);
            break;
        case 's':
        case 'S':
        case 0xe050:
            change_crawling_direct(DIRECT_DOWN);
            break;
        case 'a':
        case 'A':
        case 0xe04b:
            change_crawling_direct(DIRECT_LEFT);
            break;
        case 'd':
        case 'D':
        case 0xe04d:
            change_crawling_direct(DIRECT_RIGHT);
            break;
    }
    
    /* put an egg for the snake */
    if(!eggs)
        put_egg(1);

    return crawl();
}

int unregister_timer()
{
    struct itimerval itv;
    itv.it_interval.tv_sec = 0;
    itv.it_interval.tv_usec = 0;
    itv.it_value = itv.it_interval;

    return setitimer(ITIMER_REAL, &itv, NULL);
}

void time_tick(int signo)
{
    static int ticks = 10;
    if (ticks > snake.speed_factor) {
        ticks --;
        return;
    }
    ticks = 10;
    if(timeline_forward()) {
        game_stat = GAME_STAT_STOP;
        unregister_timer();
    }       
}

int register_timer()
{
    struct itimerval itv;
    itv.it_interval.tv_sec = 0;
    itv.it_interval.tv_usec = 10000;
    itv.it_value = itv.it_interval;

    signal(SIGALRM, time_tick);

    return setitimer(ITIMER_REAL, &itv, NULL);
}

void game_ready()
{
    draw_stage();
    snake_remove(0);
    snake_born(WIDTH / 2, HEIGHT / 2, DIRECT_RIGHT);
    score(1); /* zero total_score */
    game_stat = GAME_STAT_READY;
}

int game_play()
{
    if (game_stat != GAME_STAT_READY)
        return -1;
    
    if (!register_timer())
        game_stat = GAME_STAT_PLAYING;

    return 0;
}

void game_reset()
{
    if (game_stat == GAME_STAT_PLAYING)
        unregister_timer();
    
    reset_kb();
    snake_remove(0);

    game_stat = GAME_STAT_STOP;
}

int game_exit(int exit_code)
{
    game_reset();
    print_s("GAME OVER!", ORG_X, ORG_Y + HEIGHT + 1);

    return 0;
}

int main()
{
    game_ready();
    game_play();

    while(game_stat != GAME_STAT_STOP)
        sleep(1);
    
    return game_exit(0);
}
