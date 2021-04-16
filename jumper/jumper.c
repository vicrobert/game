/*
 * Jumper for Funny
 * Description: this game must be run in linux
 * Compile tools chain: gcc & gdb
 * Author: Junbo Yang
 * Date: 6/20/2016
 *
 * updated on 3/26/2021
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

static struct termios ori_attr, cur_attr;
static pedal_t * pedal_pool, * pedal_alloc_ptr;
static pedal_t * pedal_head = NULL, * pedal_tail = NULL;
static actor_t * actor;
static int nonblock = 0;
static short speed_factor = 0;
int total_score = 0;
int game_stat = 0;
const char * pedal_skin = "_+";
const char * actor_skin = "Y";
const char * blood_shape = "o";
const char * bg = " ";

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
    unsigned int nread;
    char key[128] = {0};

    set_stdin_nonblock();

    nread = read(STDIN_FILENO, key, 128);
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

/*
 * returns: 0 no crash
 *          1 hit the pedal
 *          2 hit the top boundary
 *          3 hit the bottom boundary
 *          4 hit the trap
 */
int __hit_test(actor_t * actor, pedal_t * ped)
{
    if (actor->y < 1)
        return HITR_TOP_BOUNDARY;
        
    if (actor->y > HEIGHT - 2)
        return HITR_BOT_BOUNDARY;

    if (actor->x >= ped->x && actor->y == ped->y
        && actor->x < ped->x + ped->length) {
            ped->actor = actor;
            actor->pedal = ped;
        if (ped->type == PEDAL_TYPE_NOR)
            return HITR_PEDAL;
        else
            return HITR_TRAP;
    } else {
        ped->actor = NULL;
        return HITR_NOCRASH;
    }
}

void actor_erase(actor_t * actor)
{
    print_s(bg, actor->x, actor->y);
}

void actor_draw(actor_t * actor)
{
    print_s(actor_skin, actor->x, actor->y);
}

void pedal_draw(pedal_t * pedal)
{
    int i;
    for (i = 0; i < pedal->length; i ++) {
        if (pedal->blood_x == pedal->x + i)
            print_c(blood_shape[0], pedal->x + i, pedal->y);
        else
            print_c(pedal_skin[pedal->type], pedal->x + i, pedal->y);
    }
        
}

void pedal_draw_slice(pedal_t * pedal, int x)
{
    print_c(pedal_skin[pedal->type], x, pedal->y);
}

void pedal_erase(pedal_t * pedal)
{
    int i;
    for (i = 0; i < pedal->length; i ++)
        print_s(bg, pedal->x + i, pedal->y);
}

void pedal_erase_slice(pedal_t * pedal, int x)
{
    print_s(bg, x, pedal->y);
}

void pedal_reset(pedal_t * pedal)
{
    if (pedal) {
        pedal->x = pedal->y = 0;
        pedal->length = 0;
        pedal->type = -1;
    }
}

void __pedal_rise(pedal_t * pedal)
{
    int hit_code;
    
    pedal_erase(pedal);
    pedal->y --;
    
    if (pedal->y >= 1) {
        pedal_draw(pedal);
        if (pedal->actor) {
            pedal->actor->y = pedal->y;
            actor_draw(pedal->actor);
            //feed blood
            if (pedal->blood_x == pedal->actor->x) {
                actor_feed_blood(pedal->actor, 1);
                pedal->blood_x = 0;
            }
        } else {
            hit_code = pedal_hit_test(pedal);
            if (hit_code == HITR_TRAP) {
                /* if hit code equals 4, the actor had been bound */
                assert(pedal->actor);
                actor_continue_life(pedal->actor);
            }
        }
    } else if (pedal->actor) {
        actor_continue_life(pedal->actor);
    }
}

int pedal_hit_test(pedal_t * pedal)
{
    return __hit_test(actor, pedal);
}

int actor_hit_test(actor_t * actor)
{
    pedal_t * cur_pd;
    int hit_code;
    list_for_each(cur_pd, pedal_head, pedal_t, sibling) {
        hit_code = __hit_test(actor, cur_pd);
        if (hit_code != HITR_NOCRASH)
            return hit_code;
    }
    actor->pedal = NULL;
    return HITR_NOCRASH;
}

int actor_move_left(actor_t * actor)
{
    pedal_t * ped = actor->pedal;
    if (actor->x > 1 && actor->y > 0 && actor->y < HEIGHT - 1) {
        if (ped) {
            if (actor->x >= ped->x
                && actor->x < ped->x + ped->length) {
                pedal_draw_slice(ped, actor->x);
            } else {
                actor_erase(actor);
            }
            //feed blood
            if (ped->blood_x == actor->x - 1) {
                actor_feed_blood(actor, 1);
                ped->blood_x = 0;
            }
        } else {
            actor_erase(actor);
        }

        actor->x --;
        actor_draw(actor);

        if (ped && (actor->x < ped->x
            || actor->x > ped->x + ped->length - 1)) {
            ped->actor = NULL;
            actor->pedal = NULL;
        }

        return 0;
    }
    return 1;
}

int actor_move_right(actor_t * actor)
{
    pedal_t * ped = actor->pedal;
    if (actor->x < WIDTH - 2 && actor->y > 0 && actor->y < HEIGHT - 1) {
        if (ped) {
            if (actor->x >= ped->x
                && actor->x < ped->x + ped->length) {
                pedal_draw_slice(ped, actor->x);
            } else {
                actor_erase(actor);
            }
            //feed blood
            if (ped->blood_x == actor->x + 1) {
                actor_feed_blood(actor, 1);
                ped->blood_x = 0;
            }
        } else {
            actor_erase(actor);
        }
        actor->x ++;
        actor_draw(actor);

        if (ped && (actor->x < ped->x
            || actor->x > ped->x + ped->length - 1)) {
            ped->actor = NULL;
            actor->pedal = NULL;
        }

        return 0;
    }
    return 1;
}

int actor_move_down(actor_t * actor)
{
    int hit_code;
    pedal_t * ped = actor->pedal;

    if (ped || actor->initialized == 0)
        return -1;

    if (actor->y <= HEIGHT - 2) {
        actor_erase(actor);
        actor->y ++;
        
        hit_code = actor_hit_test(actor);
        if (hit_code == HITR_TOP_BOUNDARY || 
            hit_code == HITR_BOT_BOUNDARY || 
            hit_code == HITR_TRAP) {
            actor_continue_life(actor);
        } else {
            actor_draw(actor);
            score(0);
        }
    }

    return 0;
}

int actor_feed_blood(actor_t * actor, int lives)
{
    actor->lives += lives;
    show_lives(actor->lives - 1);
}

int actor_continue_life(actor_t * actor)
{
    if (actor->lives > 1) {
        if (actor->pedal)
            actor->pedal->actor = NULL;
        
        actor->pedal = pedal_tail;           
        if (actor->pedal->type != PEDAL_TYPE_NOR)
            actor->pedal->type = PEDAL_TYPE_NOR;
        
        actor->x = actor->pedal->x + actor->pedal->length / 2;
        actor->y = actor->pedal->y;
        actor->pedal->actor = actor;
        actor->lives --;
        show_lives(actor->lives - 1);
        return 0;
    }
    
    game_reset();
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

    for (i = 1; i < PEDAL_POOL_MAX; i ++) {
        alloc_one = malloc(sizeof(pedal_t));
        memset(alloc_one, 0, sizeof(pedal_t));
        list_append(alloc_one, pedal_alloc_ptr, sibling);
        pedal_alloc_ptr = alloc_one;
    }
}

void __init_actor()
{
    actor = malloc(sizeof(actor_t));
    memset(actor, 0, sizeof(actor_t));
    actor->lives = ACTOR_LIVES;
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
        pedal_reset(pedal);
        list_append(pedal, pedal_alloc_ptr, sibling);
        pedal_alloc_ptr = pedal;
    }
}

void pedal_rise()
{
    pedal_t * cur;
    if (pedal_head != NULL) {
        list_for_each(cur, pedal_head, pedal_t, sibling)
            __pedal_rise(cur);

        if (pedal_head->y < 1) {
            cur = pedal_head;
            pedal_head = list_next_obj(pedal_head, pedal_t, sibling);
            reclaim_pedal(cur);
        }
    }
}

void pedal_appear()
{
    int pedal_x, pedal_len, pedal_type;
    pedal_t * new_one = alloc_pedal();
    if (new_one != NULL) {
        srand(time(0));
        pedal_len = (PEDAL_LENGTH_MAX - PEDAL_LENGTH_MIN) 
            * (rand() / (double)RAND_MAX) + PEDAL_LENGTH_MIN;
        pedal_x = (WIDTH - pedal_len - 3) 
            * (rand() / (double)RAND_MAX) + 1;
        pedal_type = rand() / (double)RAND_MAX > 0.15
            ? PEDAL_TYPE_NOR : PEDAL_TYPE_TRAP;
        if (pedal_type == PEDAL_TYPE_NOR 
            && rand() / (double)RAND_MAX < 0.06)
            new_one->blood_x = pedal_x + pedal_len 
                * (rand() / (double)RAND_MAX) - 1;
        else
            new_one->blood_x = 0;
        new_one->x = pedal_x;
        new_one->y = HEIGHT - 2;
        new_one->length = pedal_len;
        new_one->type = pedal_type;

        if (pedal_head == NULL) {
            pedal_head = new_one;
            pedal_tail = pedal_head;
        } else {
            list_append(new_one, pedal_tail, sibling);
            pedal_tail = new_one;
        }

        if (actor->initialized == 0) {
            actor->x = new_one->x + new_one->length / 2;
            actor->y = new_one->y;
            actor->pedal = new_one;
            new_one->actor = actor;
            new_one->type = PEDAL_TYPE_NOR;
            actor->initialized = 1;
            actor_draw(actor);
        }
        pedal_draw(new_one);
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
    print_s("       ", 6, HEIGHT);
    print_s(score_s, 6, HEIGHT);
    return total_score;
}

void show_lives(int lives)
{
    char lives_s[5];
    sprintf(lives_s, "%d", lives);
    print_s("  ", WIDTH - 2, HEIGHT);
    print_s(lives_s, WIDTH - 2, HEIGHT);   
}

void update_speed_factor()
{
    if (total_score > 5000)
        speed_factor = 3;
    else if (total_score > 2000)
        speed_factor = 2;
    else if (total_score > 800)
        speed_factor = 1;
}

void draw_stage()
{
    int i;

    system("clear");
    /* drawing vertical line */
    for (i = 1; i < HEIGHT - 1; ++ i) {
        print_s("#", 0, i);
        print_s("#", WIDTH - 1, i);
    }
    /* drawing horizontal line */
    for (i = 0; i < WIDTH; ++ i) {
        print_s("#", i, 0);
        print_s("#", i, HEIGHT - 1);
    }
    /* show score label */
    print_s("SCORE:", 0, HEIGHT);
    
    /* show lives label */
    print_s("LIVES:", WIDTH - 8, HEIGHT);
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
            //TODO: UP
            break;
        case 's':
        case 'S':
        case 0xe050:
            //TODO: DOWN
            break;
        case 'a':
        case 'A':
        case 0xe04b:
            actor_move_left(actor);
            break;
        case 'd':
        case 'D':
        case 0xe04d:
            actor_move_right(actor);
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
    static int ticks_6 = 6;
    if (ticks_6 > speed_factor) {
        ticks_6 --;
        return;
    }
    ticks_6 = 6;
    actor_move_down(actor);
}

void pedal_rise_trigger()
{
    static int ticks_10 = 10;
    if (ticks_10 > speed_factor) {
        ticks_10 --;
        return;
    }
    ticks_10 = 10;

    pedal_rise();
}

void pedal_appear_trigger()
{
    static int ticks_100 = 100;
    if (ticks_100 > 0) {
        ticks_100 --;
        return;
    }
    ticks_100 = 100;
    
    pedal_appear();
}

void game_tuning_trigger()
{
    static int ticks_100 = 100;
    if (ticks_100 > 0) {
        ticks_100 --;
        return;
    }
    ticks_100 = 100;
    update_speed_factor();
}

void time_forward(int signo)
{
    exec_input_cmd();
    
    if (game_stat == GAME_STAT_PAUSE)
        return;
    
    pedal_appear_trigger();
    pedal_rise_trigger();
    actor_fall_trigger();
    game_tuning_trigger();
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
    show_lives(ACTOR_LIVES - 1); /* reset actor lives */
    game_stat = GAME_STAT_READY;
}

int game_play()
{
    if (game_stat != GAME_STAT_READY)
        return -1;

    if (!register_timer())
        game_stat = GAME_STAT_PLAYING;

    while(game_stat != GAME_STAT_STOP)
        sleep(1);

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
    print_s("GAME OVER!", 0, HEIGHT + 1);

    return 0;
}

int main()
{
    initialize();
    game_ready();
    game_play();

    return game_exit(0);
}
