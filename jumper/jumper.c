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
#include "jumper.h"
#include "list.h"

#define WIDTH     50
#define HEIGHT    35
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

#define print_s(s, x, y) \
    printf("\033[%d;%dH%s\n", (y), (x), (s))

#define print_i(i, x, y) \
    printf("\033[%d;%dH%d\n", (y), (x), (i))

static struct termios ori_attr, cur_attr;
static pedal_t * pedal_pool, * pedal_alloc_ptr;
static pedal_t * pedal_head = NULL, * pedal_tail = NULL;
static actor_t * actor;
static int nonblock = 0;
static short speed_factor = 0;
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

void __pedal_draw(pedal_t * this)
{
    int i;
    for (i = 0; i < this->length; i ++)
        print_s("_", this->x + i, this->y);
}

void __pedal_draw_slice(pedal_t * this, int x)
{
    print_s("_", x, this->y);
}

void __pedal_erase(pedal_t * this)
{
    int i;
    for (i = 0; i < this->length; i ++)
        print_s(" ", this->x + i, this->y);
}

void __pedal_erase_slice(struct _pedal_t * this, int x)
{
    print_s(" ", x, this->y);
}

void __pedal_reset(pedal_t * pedal)
{
    if (pedal) {
        pedal->x = pedal->y = 0;
        pedal->length = 0;
        pedal->type = -1;
    }
}

void __pedal_rise(pedal_t * this)
{
    this->erase(this);
    this->y --;
    if (this->y > ORG_Y) {
        this->draw(this);
        if (this->actor) {
            this->actor->y = this->y;
            this->actor->draw(this->actor);
        }
    }
}

void __actor_erase(actor_t * this)
{
    print_s(" ", this->x, this->y);
}

void __actor_draw(actor_t * this)
{
    print_s("Y", this->x, this->y);
}

/*
 * returns: 0 no crash
 *          1 hit the pedal
 *          2 hit the boundary line
 */
int hit_test(actor_t * actor, pedal_t * ped)
{
    if (actor->y > ORG_Y + HEIGHT - 1)
        return 2;

    if (actor->x >= ped->x && actor->x < ped->x + ped->length
        && actor->y == ped->y) {
        ped->actor = actor;
        actor->pedal = ped;
        return 1;
    } else {
        ped->actor = NULL;
        return 0;
    }
}

void ___determine_hit(actor_t * actor)
{
    pedal_t * cur_pd;
    list_for_each(cur_pd, pedal_head, pedal_t, sibling)
        if (hit_test(actor, cur_pd) == 1)
            return;
    actor->pedal = NULL;
}

int __actor_move_left(actor_t * this)
{
    pedal_t * ped = this->pedal;
    if (this->x > ORG_X + 1) {
        if (ped) {
            if (this->x >= ped->x
                && this->x < ped->x + ped->length) {
                ped->draw_slice(ped, this->x);
            } else {
                this->erase(this);
            }
        } else {
            this->erase(this);
        }

        this->x --;
        this->draw(this);

        if (ped && (this->x < ped->x
            || this->x > ped->x + ped->length - 1)) {
            ped->actor = NULL;
            this->pedal = NULL;
        }

        return 0;
    }
    return 1;
}

int __actor_move_right(actor_t * this)
{
    pedal_t * ped = this->pedal;
    if (this->x < ORG_X + WIDTH - 2) {
        if (ped) {
            if (this->x >= ped->x
                && this->x < ped->x + ped->length) {
                ped->draw_slice(ped, this->x);
            } else {
                this->erase(this);
            }
        } else {
            this->erase(this);
        }
        this->x ++;
        this->draw(this);

        if (ped && (this->x < ped->x
            || this->x > ped->x + ped->length - 1)) {
            ped->actor = NULL;
            this->pedal = NULL;
        }

        return 0;
    }
    return 1;
}

int __actor_move_up(actor_t * this)
{
    pedal_t * ped = this->pedal;
    if (this->y > ORG_Y + 1) {
        if (ped) {
            if (this->x >= ped->x
                && this->x < ped->x + ped->length) {
                ped->draw_slice(ped, this->x);
            } else {
                this->erase(this);
            }
        } else {
           this->erase(this);
        }
        this->y --;
        this->draw(this);

        return 0;
    }
    return -1;
}

int __actor_move_down(actor_t * this)
{
    pedal_t * ped = this->pedal;

    if (ped || this->initialized == 0)
        return -1;

    if (this->y < ORG_Y + HEIGHT - 2) {
        this->erase(this);
        this->y ++;
        this->draw(this);

        ___determine_hit(this);

        return 0;
    }
    return -1;
}

void __init_pedal_pool()
{
    int i;
    pedal_t * alloc_one;

    pedal_pool = malloc(sizeof(pedal_t));
    memset(pedal_pool, 0, sizeof(pedal_t));
    pedal_alloc_ptr = pedal_pool;
    list_init_listhead(pedal_pool, sibling);
    pedal_alloc_ptr->erase = __pedal_erase;
    pedal_alloc_ptr->erase_slice = __pedal_erase_slice;
    pedal_alloc_ptr->draw = __pedal_draw;
    pedal_alloc_ptr->draw_slice = __pedal_draw_slice;
    pedal_alloc_ptr->reset = __pedal_reset;
    pedal_alloc_ptr->rise = __pedal_rise;

    for (i = 1; i < PEDAL_LENGTH_MAX; i ++) {
        alloc_one = malloc(sizeof(pedal_t));
        memset(alloc_one, 0, sizeof(pedal_t));
        alloc_one->erase = __pedal_erase;
        alloc_one->erase_slice = __pedal_erase_slice;
        alloc_one->draw = __pedal_draw;
        alloc_one->draw_slice = __pedal_draw_slice;
        alloc_one->reset = __pedal_reset;
        alloc_one->rise = __pedal_rise;
        list_append(alloc_one, pedal_alloc_ptr, sibling);
        pedal_alloc_ptr = alloc_one;
    }
}

void __init_actor()
{
    actor = malloc(sizeof(actor_t));
    memset(actor, 0, sizeof(actor_t));
    actor->erase = __actor_erase;
    actor->draw = __actor_draw;
    actor->move_left = __actor_move_left;
    actor->move_right = __actor_move_right;
    actor->move_up = __actor_move_up;
    actor->move_down = __actor_move_down;
}

void __free_pedal_pool()
{
    pedal_t * cur, * to_free;
    list_for_each(cur, pedal_pool, pedal_t, sibling) {
        to_free = list_prev_obj(cur, pedal_t, sibling);
        if (to_free)
            free(to_free);
    }
    free(pedal_alloc_ptr);
    pedal_head = pedal_alloc_ptr = NULL;
}

void __free_actor()
{
    if (actor) {
        free(actor);
        actor = NULL;
    }
}

pedal_t * alloc_pedal()
{
    pedal_t * pick = pedal_alloc_ptr;
    if (pick) {
        pedal_alloc_ptr = list_prev_obj(pick, pedal_t, sibling);
        list_remove(pick, sibling);
    }
    return pick;
}

int reclaim_pedal(pedal_t * pedal)
{
    if (pedal) {
        pedal->reset(pedal);
        list_append(pedal, pedal_alloc_ptr, sibling);
        pedal_alloc_ptr = pedal;
    }
}

void pedal_rise()
{
    pedal_t * cur;
    if (pedal_head != NULL) {
        list_for_each(cur, pedal_head, pedal_t, sibling) {
            cur->rise(cur);
            if (!actor->pedal)
                hit_test(actor, cur);
        }

        if (pedal_head->y < ORG_Y + 1) {
            cur = pedal_head;
            pedal_head = list_next_obj(pedal_head, pedal_t, sibling);
            reclaim_pedal(cur);
        }
    }
}

void pedal_appear()
{
    int pedal_x, pedal_len;
    pedal_t * new_one = alloc_pedal();
    if (new_one != NULL) {
        srand(time(0));
        pedal_len = (PEDAL_LENGTH_MAX - 3) * (rand() / (RAND_MAX + 1.0)) + 3;
        pedal_x = (WIDTH - pedal_len - 3 ) * (rand() / (RAND_MAX + 1.0)) + 1;
        new_one->x = pedal_x + ORG_X;
        new_one->y = ORG_X + HEIGHT - 2;
        new_one->length = pedal_len;

        if (pedal_head == NULL) {
            pedal_head = new_one;
            pedal_tail = pedal_head;
        } else {
            list_append(new_one, pedal_tail, sibling);
            pedal_tail = new_one;
        }

        if (actor->initialized == 0) {
            actor->x = new_one->x;
            actor->y = new_one->y;
            actor->pedal = new_one;
            new_one->actor = actor;
            actor->initialized = 1;
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

int exec_input_cmd()
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
            // change_crawling_direct(DIRECT_UP;
            break;
        case 's':
        case 'S':
        case 0xe050:
            // change_crawling_direct(DIRECT_DOWN);
            break;
        case 'a':
        case 'A':
        case 0xe04b:
            actor->move_left(actor);
            break;
        case 'd':
        case 'D':
        case 0xe04d:
            actor->move_right(actor);
            break;
    }

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

void actor_fall_trigger()
{
    //TODO:调速
    actor->move_down(actor);
}

void pedal_rise_trigger()
{
    //TODO:调速
    pedal_rise();
}

void pedal_appear_trigger()
{
    static int ticks_10x12 = 12;
    if (ticks_10x12 > 0) {
        ticks_10x12 --;
        return;
    }
    ticks_10x12 = 12;
    pedal_appear();
}

void time_forward(int signo)
{
    static int ticks_10 = 10;
    if (ticks_10 > speed_factor) {
        ticks_10 --;
        return;
    }
    ticks_10 = 10;

    if(exec_input_cmd()) {
        game_stat = GAME_STAT_STOP;
        unregister_timer();
    }

    actor_fall_trigger();
    pedal_rise_trigger();
    pedal_appear_trigger();
}

int register_timer()
{
    struct itimerval itv;
    itv.it_interval.tv_sec = 0;
    itv.it_interval.tv_usec = 10000;
    itv.it_value = itv.it_interval;

    signal(SIGALRM, time_forward);

    return setitimer(ITIMER_REAL, &itv, NULL);
}

void initialize()
{
    __init_pedal_pool();
    __init_actor();
}

void uninitialize()
{
    __free_pedal_pool();
    __free_actor();
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
    uninitialize();
    print_s("GAME OVER!", ORG_X, ORG_Y + HEIGHT + 1);

    return 0;
}

int main()
{
    initialize();
    game_ready();
    game_play();

    while(game_stat != GAME_STAT_STOP)
        sleep(1);

    return game_exit(0);
}
