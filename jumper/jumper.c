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
int ___hit_test(actor_t * actor, pedal_t * ped)
{
    if (actor->y < 1)
        return 2;
        
    if (actor->y > HEIGHT - 2)
        return 3;

    if (actor->x >= ped->x && actor->x < ped->x + ped->length
        && actor->y == ped->y) {
            ped->actor = actor;
            actor->pedal = ped;
        if (ped->type == PEDAL_TYPE_NOR)
            return 1;
        else
            return 4;
    } else {
        ped->actor = NULL;
        return 0;
    }
}

void __pedal_draw(pedal_t * this)
{
    int i;
    for (i = 0; i < this->length; i ++) {
        if (this->blood_x == this->x + i)
            print_c(blood_shape[0], this->x + i, this->y);
        else
            print_c(pedal_skin[this->type], this->x + i, this->y);
    }
        
}

void __pedal_draw_slice(pedal_t * this, int x)
{
    print_c(pedal_skin[this->type], x, this->y);
}

void __pedal_erase(pedal_t * this)
{
    int i;
    for (i = 0; i < this->length; i ++)
        print_s(bg, this->x + i, this->y);
}

void __pedal_erase_slice(struct _pedal_t * this, int x)
{
    print_s(bg, x, this->y);
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
    int hit_code;
    
    this->erase(this);
    this->y --;
    
    if (this->y >= 1) {
        this->draw(this);
        if (this->actor) {
            this->actor->y = this->y;
            this->actor->draw(this->actor);
            //feed blood
            if (this->blood_x == this->actor->x) {
                this->actor->feed_blood(this->actor, 1);
                this->blood_x = 0;
            }
        } else {
            hit_code = this->hit_test(this);
            if (hit_code == 4) {
                /* if hit code equals 4, the actor had been bound */
                assert(this->actor);
                this->actor->continue_life(this->actor);
            }
        }
    } else if (this->actor) {
        this->actor->continue_life(this->actor);
    }
}

int __pedal_hit_test(pedal_t * this)
{
    return ___hit_test(actor, this);
}

void __actor_erase(actor_t * this)
{
    print_s(bg, this->x, this->y);
}

void __actor_draw(actor_t * this)
{
    print_s(actor_skin, this->x, this->y);
}

int __actor_hit_test(actor_t * this)
{
    pedal_t * cur_pd;
    int hit_code;
    list_for_each(cur_pd, pedal_head, pedal_t, sibling) {
        hit_code = ___hit_test(this, cur_pd);
        if (hit_code != 0)
            return hit_code;
    }
    actor->pedal = NULL;
    return 0;
}

int __actor_move_left(actor_t * this)
{
    pedal_t * ped = this->pedal;
    if (this->x > 1 && this->y > 0 && this->y < HEIGHT - 1) {
        if (ped) {
            if (this->x >= ped->x
                && this->x < ped->x + ped->length) {
                ped->draw_slice(ped, this->x);
            } else {
                this->erase(this);
            }
            //feed blood
            if (ped->blood_x == this->x - 1) {
                this->feed_blood(this, 1);
                ped->blood_x = 0;
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
    if (this->x < WIDTH - 2 && this->y > 0 && this->y < HEIGHT - 1) {
        if (ped) {
            if (this->x >= ped->x
                && this->x < ped->x + ped->length) {
                ped->draw_slice(ped, this->x);
            } else {
                this->erase(this);
            }
            //feed blood
            if (ped->blood_x == this->x + 1) {
                this->feed_blood(this, 1);
                ped->blood_x = 0;
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
    if (this->y > 1) {
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
    int hit_code;
    pedal_t * ped = this->pedal;

    if (ped || this->initialized == 0)
        return -1;

    if (this->y <= HEIGHT - 2) {
        this->erase(this);
        this->y ++;
        
        hit_code = this->hit_test(this);
        if (hit_code == 2 || hit_code == 3 || hit_code == 4) {
            this->continue_life(this);
        } else {
            this->draw(this);
            score(0);
        }
    }

    return 0;
}

int __actor_feed_blood(actor_t * this, int lives)
{
    this->lives += lives;
    show_lives(this->lives - 1);
}

int __actor_continue_life(actor_t * this)
{
    if (this->lives > 1) {
        if (this->pedal)
            this->pedal->actor = NULL;
        
        this->pedal = pedal_tail;           
        if (this->pedal->type != PEDAL_TYPE_NOR)
            this->pedal->type = PEDAL_TYPE_NOR;
        
        this->x = (2 * this->pedal->x + this->pedal->length) / 2;
        this->y = this->pedal->y;
        this->pedal->actor = this;
        this->lives --;
        show_lives(this->lives - 1);
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
    pedal_alloc_ptr->erase = __pedal_erase;
    pedal_alloc_ptr->erase_slice = __pedal_erase_slice;
    pedal_alloc_ptr->draw = __pedal_draw;
    pedal_alloc_ptr->draw_slice = __pedal_draw_slice;
    pedal_alloc_ptr->reset = __pedal_reset;
    pedal_alloc_ptr->rise = __pedal_rise;
    pedal_alloc_ptr->hit_test = __pedal_hit_test;

    for (i = 1; i < PEDAL_POOL_MAX; i ++) {
        alloc_one = malloc(sizeof(pedal_t));
        memset(alloc_one, 0, sizeof(pedal_t));
        alloc_one->erase = __pedal_erase;
        alloc_one->erase_slice = __pedal_erase_slice;
        alloc_one->draw = __pedal_draw;
        alloc_one->draw_slice = __pedal_draw_slice;
        alloc_one->reset = __pedal_reset;
        alloc_one->rise = __pedal_rise;
        alloc_one->hit_test = __pedal_hit_test;
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
    actor->hit_test = __actor_hit_test;
    actor->continue_life = __actor_continue_life;
    actor->feed_blood = __actor_feed_blood;
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
        pedal->reset(pedal);
        list_append(pedal, pedal_alloc_ptr, sibling);
        pedal_alloc_ptr = pedal;
    }
}

void pedal_rise()
{
    pedal_t * cur;
    if (pedal_head != NULL) {
        list_for_each(cur, pedal_head, pedal_t, sibling)
            cur->rise(cur);

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
            actor->x = (2 * new_one->x + new_one->length) / 2;
            actor->y = new_one->y;
            actor->pedal = new_one;
            new_one->actor = actor;
            new_one->type = PEDAL_TYPE_NOR;
            actor->initialized = 1;
            actor->draw(actor);
        }
        new_one->draw(new_one);
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
    static int ticks_6 = 6;
    if (ticks_6 > speed_factor) {
        ticks_6 --;
        return;
    }
    ticks_6 = 6;
    actor->move_down(actor);
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

    while(game_stat != GAME_STAT_STOP)
        sleep(1);

    return game_exit(0);
}
