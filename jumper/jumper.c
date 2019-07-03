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
#define PEDAL_POOL_MAX  10
#define PEDAL_LENGTH_MAX 10

struct pedal
{
    /* function table */
    void (* reset)(struct pedal * this);
    void (* shift_upstair)(struct pedal * this);
    /* data fields */
    int x;
    int y;
    int length;
    char type;
    struct pedal *prev_ptr;
    struct pedal *next_ptr;
};


#define print_s(s, x, y) \
    printf("\033[%d;%dH%s\n", (y), ((x) << 1), (s))

#define print_i(i, x, y) \
    printf("\033[%d;%dH%d\n", (y), (x), (i))

static struct termios ori_attr, cur_attr;
static struct pedal pedal_pool[PEDAL_POOL_MAX];
static struct pedal * pedal_head = NULL, * pedal_tail = NULL;
static int allc_idx = 0;
static int recl_idx = -1;
static int nonblock = 0;
static short speed_factor;
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

struct pedal * __alloc_pedal()
{
    struct pedal * new;

    if (allc_idx == recl_idx)
        return NULL;

    new = &pedal_pool[allc_idx];

    if (recl_idx == -1)
        recl_idx = allc_idx;

    allc_idx = (allc_idx + 1) % PEDAL_POOL_MAX;

    return new;
}

int __reclaim_pedal(struct pedal ** pedal)
{
    if (pedal != NULL && * pedal != NULL) {
        if (recl_idx == -1)
            return;

        (* pedal)->reset(* pedal);

        recl_idx = (recl_idx + 1) % PEDAL_POOL_MAX;

        if (recl_idx == allc_idx)
            recl_idx = -1;

        * pedal = NULL;

        return 0;
    }
    return -1;
}

void __pedal_reset(struct pedal * this)
{
    memset((void *)this, NULL, sizeof(this));
}

void __pedal_shift_upstair(struct pedal * this)
{
    int i;
    char wdc[4], c[20];
    // sprintf(wdc, "%%%ds", this->length);
    // sprintf(c, wdc, " ");
    // print_s(c, this->x, this->y);
    for (i = 0; i < this->length; i ++)
        print_s(" ", this->x + i, this->y);
    this->y --;
    if (this->y > 0) {
        for (i = 0; i < this->length; i ++)
            print_s("_", this->x + i, this->y);
    }
}

void shift_upstair()
{
    struct pedal * cur = pedal_head;
    if (pedal_head != NULL) {
        while (cur != NULL) {
            cur->shift_upstair(cur);
            cur = cur->next_ptr;
        }
        if (pedal_head->y < 1) {
            cur = pedal_head->next_ptr;
            __reclaim_pedal(&pedal_head);
            pedal_head = cur;
        }
    }
}

void pedal_appear()
{
    int pedal_x, pedal_len;
    struct pedal * new_one = __alloc_pedal();
    if (new_one != NULL) {
        new_one->reset = __pedal_reset;
        new_one->shift_upstair = __pedal_shift_upstair;

        srand(time(0));
        pedal_x = (WIDTH - 2) * (rand() / (RAND_MAX + 1.0));
        pedal_len = PEDAL_LENGTH_MAX * (rand() / (RAND_MAX + 1.0));
        new_one->x = pedal_x + ORG_X;
        new_one->y = HEIGHT - 2;
        new_one->length = 10;
        new_one->next_ptr = NULL;

        if (pedal_head == NULL) {
            new_one->prev_ptr = NULL;
            pedal_head = new_one;
            pedal_tail = pedal_head;
        } else {
            new_one->prev_ptr = pedal_tail;
            pedal_tail->next_ptr = new_one;
            pedal_tail = new_one;
        }
    }
}

int score(int clean)
{
    char score_s[20];
    /* calculate */
    if (clean)
        total_score = 0;
    else
        total_score += speed_factor + 1;
    /* show */
    sprintf(score_s, "%d", total_score);
    print_s("                    ", ORG_Y + 6, ORG_Y + HEIGHT);
    print_s(score_s, ORG_Y + 6, ORG_Y + HEIGHT);

    return total_score;
}










// int snake_hit_test()
// {
//     int i, j;
//
//     if (!snake.head || !snake.tail)
//         return -1;
//
//     j = snake.head->x - ORG_X;
//     i = snake.head->y - ORG_Y;
//     if(stage_map[i][j] == 2)
//         return 1;
//
//     struct body_slice_type *pslice = snake.head->next_slice_ptr;
//     while(pslice) {
//         if (snake.head->x == pslice->x && snake.head->y == pslice->y)
//             return 1;
//         pslice = pslice->next_slice_ptr;
//     }
//
//     return 0;
// }

void update_speed_factor()
{
    // double range = (WIDTH - 2) * (HEIGHT - 2) / GAME_SPEED_ACC_FACT;
    // int len = (int)(snake.length > range? range: snake.length);
    // speed_factor = (int)(10 * len / range);
}


void draw_stage()
{
    int i, j;

    system("clear");
    /* drawing vertical line */
    for (i = ORG_Y + 1, j = 1; i < ORG_Y + HEIGHT - 1; ++ i, ++ j) {
        print_s("#", ORG_X, i);
        print_s("#", ORG_X + WIDTH - 1, i);
    }
    /* drawing horizontal line */
    for (i = ORG_X, j = 0; i < ORG_X + WIDTH; ++ i, ++ j) {
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
            // change_crawling_direct(DIRECT_UP);
            break;
        case 's':
        case 'S':
        case 0xe050:
            // change_crawling_direct(DIRECT_DOWN);
            break;
        case 'a':
        case 'A':
        case 0xe04b:
            // change_crawling_direct(DIRECT_LEFT);
            break;
        case 'd':
        case 'D':
        case 0xe04d:
            // change_crawling_direct(DIRECT_RIGHT);
            break;
    }

    shift_upstair();
    return 0;
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
    if (ticks > speed_factor) {
        ticks --;
        return;
    }
    ticks = 10;
    if(timeline_forward()) {
        game_stat = GAME_STAT_STOP;
        unregister_timer();
    }

    static int ticks_2 = 12;
    if (ticks_2 > speed_factor) {
        ticks_2 --;
        return;
    }
    ticks_2 = 12;
    pedal_appear();
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
