#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <fcntl.h>
#include <wait.h>
#include <unistd.h>
#include <libgen.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#define PROC_COUNT (9)
#define STARTING_PROC_ID (1)
#define MAX_CHILDS_COUNT (3)
#define MAX_USR_COUNT (101)

//#define RECV

//#define DEBUG_PIDS
//#define DEBUG_HANDS
//#define DEBUG_SIGS

const unsigned char CHILDS_COUNT[PROC_COUNT] =
{
/*  0  1  2  3  4  5  6  7  8  */
    1, 3, 2, 0, 0, 0, 1, 1, 0
};

const unsigned char CHILDS_IDS[PROC_COUNT][MAX_CHILDS_COUNT] =
{
    {1},        /* 0 */
    {2, 3, 4},  /* 1 */
    {5, 6},     /* 2 */
    {0},        /* 3 */
    {0},        /* 4 */
    {0},        /* 5 */
    {7},        /* 6 */
    {8},        /* 7 */
    {0}         /* 8 */
};

/* Group types:
 *
 * 0 = pid;
 * 1 = parent's pgid
 * 2 = previous child group
 */
const unsigned char GROUP_TYPE[PROC_COUNT] =
{
/*  0  1  2  3  4  5  6  7  8  */
    0, 1, 0, 2, 0, 0, 0, 1, 1
};

void print_receive_message(int signum);
void print_sent_message(int signum);
void sig_handler1(int signum, siginfo_t *siginfo, void *ucontext);
void sig_handler2(int signum, siginfo_t *siginfo, void *ucontext);
void sig_handler3(int signum, siginfo_t *siginfo, void *ucontext);
void sig_handler4(int signum, siginfo_t *siginfo, void *ucontext);
void sig_handler6(int signum, siginfo_t *siginfo, void *ucontext);
void sig_handler7(int signum, siginfo_t *siginfo, void *ucontext);
void sig_handler8(int signum, siginfo_t *siginfo, void *ucontext);
void set_sig_handler(void (*handler)(int, siginfo_t *, void *), int sig_no, int flags);

char *exec_name = NULL;
void print_error_exit(const char *s_name, const char *msg, const int proc_num);

int proc_id = 0;
void forker(int curr_number, int childs_count);

void kill_wait_for_children();
void wait_for_children();

pid_t *pids_list = NULL;

int main(int argc, char *argv[])
{
    exec_name = basename(argv[0]);

    pids_list = (pid_t*)mmap(pids_list, (2*PROC_COUNT)*sizeof(pid_t), PROT_READ | PROT_WRITE,
                             MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    int i = 0;                           // initialize pids list with zeros [1]
    for (i = 0; i < 2*PROC_COUNT; ++i) {
        pids_list[i] = 0;
    }

    forker(0, CHILDS_COUNT[0]);          // create processes tree [2]

    if (proc_id == 0) {                  // main process waits [3]
        wait_for_children();
        munmap(pids_list, (2*PROC_COUNT)*sizeof(pid_t));
        return 0;
    }

    on_exit(&wait_for_children, NULL);

    pids_list[proc_id] = getpid();          // save pid to list

#ifdef DEBUG_PIDS
    printf("%d\tpid: %d\tppid: %d\tpgid: %d\n", proc_id, getpid(), getppid(), getpgid(0));
    fflush(stdout);
#endif

    if (proc_id == STARTING_PROC_ID) {
        do{
            for (i = 1; (i <= PROC_COUNT) && (pids_list[i] != 0); ++i) {

            if (pids_list[i] == -1) {
                print_error_exit(exec_name, "Error: not all processes forked or initialized.\nHalting.", 0);
                exit(1);
                }
            }
        } while (i < PROC_COUNT);

#ifdef DEBUG_PIDS
        printf("All pids are set!\n");
        for (i = 0; (i < 2*PROC_COUNT); ++i) {
            printf("%d - %d\n", i, pids_list[i]);
            fflush(stdout);
        }
#endif
    }

    set_sig_handler(&kill_wait_for_children, SIGTERM, 0);

    switch (proc_id) {
        case 0:
            return; //error case
            break;
        case 1:
            set_sig_handler(&sig_handler1, SIGUSR2, SA_SIGINFO);
            break;
        case 2:
            set_sig_handler(&sig_handler2, SIGUSR1, SA_SIGINFO);
            break;
        case 3:
            set_sig_handler(&sig_handler3, SIGUSR1, SA_SIGINFO);
            break;
        case 4:
            set_sig_handler(&sig_handler4, SIGUSR1, SA_SIGINFO);
            set_sig_handler(&sig_handler4, SIGUSR2, SA_SIGINFO);
            break;
        case 5:
            break; //skip
        case 6:
            set_sig_handler(&sig_handler6, SIGUSR1, SA_SIGINFO);
            break;
        case 7:
            set_sig_handler(&sig_handler7, SIGUSR1, SA_SIGINFO);
            break;
        case 8:
            set_sig_handler(&sig_handler8, SIGUSR1, SA_SIGINFO);
            break;
        default:
            print_error_exit(exec_name, "Can't set sighandler!", proc_id);
    }

    if (proc_id == STARTING_PROC_ID) {                  // starter waits for all until all handlers are set [3]

        do {
            for (i = 1+PROC_COUNT; (i < 2*PROC_COUNT)  && (pids_list[i] != 0); ++i) {

                if (pids_list[i] == -1) {
                    print_error_exit(exec_name, "Error: not all processes forked or initialized.\nHalting.", 0);
                    exit(1);
                }
            }
        } while (i < 2*PROC_COUNT);

        pids_list[0] = 1;           // all handlers are set

        for (i = PROC_COUNT+1; i < 2*PROC_COUNT; ++i) {     /*  reset flags  */
            pids_list[i] = 0;
        }

#ifdef DEBUG_HANDS
        printf("All handlers are set!\n");
        for (i = 0; (i < 2*PROC_COUNT); ++i) {
            printf("%d - %d\n", i, pids_list[i]);
            fflush(stdout);
        }
        puts("==================================");
#endif

        sig_handler1(0, NULL, NULL); // start signal-flow

    } else {    // other processes

        do {
            // wait for all handlers setting
        } while (pids_list[0] == 0);

    }

    while (1) {
        pause();  // start cycle
    }

    return 0;
}   /*  main   */


void print_error_exit(const char *s_name, const char *msg, const int proc_num) {
    fprintf(stderr, "%s: %s %d\n", s_name, msg, proc_num);
    fflush(stderr);

    pids_list[proc_num] = -1;

    exit(1);
}   /*  print_error */


void wait_for_children() {
    int i = CHILDS_COUNT[proc_id];
    while (i > 0) {
        wait(NULL);
        --i;
    }
}   /*  wait_for_children  */


/* ====================================================== */

long long current_time() {
    struct timeval time;
    gettimeofday(&time, NULL);

    return time.tv_usec / 1000;
}   /*  current_time  */

volatile int usr_amount[2][2] =
{
/*   r, s   */
    {0, 0}, /* SIGUSR1 */
    {0, 0}  /* SIGUSR2 */
};

volatile int locker = 0;

void kill_wait_for_children() {
    int i = 0;

    for (i = 0; i < CHILDS_COUNT[proc_id]; ++i) {
#ifdef DEBUG_PIDS
        printf("sigint -> %d\n", CHILDS_IDS[proc_id][i]);
        fflush(stdout);
#endif
        kill(pids_list[CHILDS_IDS[proc_id][i]], SIGTERM);
    }

    wait_for_children();

    if (proc_id != 0)
#ifdef DEBUG_SIGS
        printf("%lld %d was terminated after %d SIGUSR1 and %d SIGUSR2\n",
               current_time(), proc_id, usr_amount[0][1], usr_amount[1][1]);
#else
    printf("%d %d завершил работу после %d SIGUSR1 и %d SIGUSR2\n",
#ifndef RECV
           getpid(), getppid(), usr_amount[0][1], usr_amount[1][1]);
#else
           getpid(), getppid(), usr_amount[0][0], usr_amount[1][0]);
#endif
    fflush(stdout);

#endif
    exit(0);
}   /*  kill_wait_for_children  */

void print_receive_message(int signum) {

    signum = (signum == SIGUSR1) ? 1 : 2;

#ifdef DEBUG_SIGS
    printf("%lld %d received %s%d\n", current_time(), proc_id,"USR", signum);
#else
    printf("%d %d %d получил %s%d %lld\n", proc_id, getpid(), getppid(),
            "USR", signum, current_time() );
#endif
    fflush(stdout);
}

void print_sent_message(int signum) {

    signum = (signum == SIGUSR1) ? 1 : 2;

#ifdef DEBUG_SIGS
    printf("%lld %d sent %s%d\n", current_time(), proc_id, "USR", signum);
#else
    printf("%d %d %d послал %s%d %lld\n", proc_id, getpid(), getppid(),
           "USR", signum, current_time() );
#endif
    fflush(stdout);
}

void sig_handler1(int signum, siginfo_t *siginfo, void *ucontext) {

    pids_list[PROC_COUNT + 7] = pids_list[PROC_COUNT + 6] = pids_list[PROC_COUNT + 4] = 0;

    if (signum == SIGUSR2) {
        ++usr_amount[1][0];
        print_receive_message(signum);
    }

    if (usr_amount[0][0] + usr_amount[1][0] == MAX_USR_COUNT) {
        kill_wait_for_children();
    }

    ++usr_amount[0][1];
    print_sent_message(SIGUSR1);
    kill(-getpgid(pids_list[6]), SIGUSR1);
}

void sig_handler2(int signum, siginfo_t *siginfo, void *ucontext) {

    if (signum == SIGUSR1) {
        ++usr_amount[0][0];
        print_receive_message(signum);
    }

    ++usr_amount[1][1];
    print_sent_message(SIGUSR2);
    kill(pids_list[1], SIGUSR2);
}

void sig_handler3(int signum, siginfo_t *siginfo, void *ucontext) {

    if (signum == SIGUSR1) {
        ++usr_amount[0][0];
        print_receive_message(signum);
    }
}

void sig_handler4(int signum, siginfo_t *siginfo, void *ucontext) {

    locker++;
    pids_list[PROC_COUNT + 4] = 1;
    if (signum == SIGUSR1) {
        ++usr_amount[0][0];
    }

    if (signum == SIGUSR2) {
        ++usr_amount[1][0];
    }

    print_receive_message(signum);

    if ( locker != 3) {
#ifdef DEBUG_SIGS
        printf("%lld %d not enough!\n",current_time(), proc_id);
        fflush(stdout);
#endif
        //locker++;
        return;
    }

    locker = 0;
    ++usr_amount[0][1];
    print_sent_message(SIGUSR1);
    kill(-getpgid(pids_list[2]), SIGUSR1);
}

void sig_handler6(int signum, siginfo_t *siginfo, void *ucontext) {

    if (signum == SIGUSR1) {
        ++usr_amount[0][0];
        print_receive_message(signum);
    }

    ++usr_amount[0][1];
    print_sent_message(SIGUSR1);
    kill(pids_list[4], SIGUSR1);

    pids_list[PROC_COUNT + 6] = 1;
}

void sig_handler7(int signum, siginfo_t *siginfo, void *ucontext) {

    if (signum == SIGUSR1) {
        ++usr_amount[0][0];
        print_receive_message(signum);
    }

    do{
        // wait for 6th and 4th processes to send signal
    } while ( (pids_list[PROC_COUNT + 6] + pids_list[PROC_COUNT + 4]) != 2 );

    ++usr_amount[1][1];
    print_sent_message(SIGUSR2);
    kill(pids_list[4], SIGUSR2);

    pids_list[PROC_COUNT + 7] = 1;
}

void sig_handler8(int signum, siginfo_t *siginfo, void *ucontext) {

    if (signum == SIGUSR1) {
        ++usr_amount[0][0];
        print_receive_message(signum);
    }

    do{
        // wait for 7th, 6th and 4th processes to send signal
    } while ( (pids_list[PROC_COUNT + 7] + pids_list[PROC_COUNT + 6] + pids_list[PROC_COUNT + 4]) != 3 );

    ++usr_amount[0][1];
    print_sent_message(SIGUSR1);
    kill(pids_list[4], SIGUSR1);
}

void set_sig_handler(void (*handler)(int, siginfo_t *, void *), int sig_no, int flags) {
    struct sigaction sa, oldsa;             // set sighandler [4]

    sigset_t block_mask;
    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGUSR1);
    sigaddset(&block_mask, SIGUSR2);

    sa.sa_mask = block_mask;
    sa.sa_flags = flags;

    if ((sa.sa_flags & SA_SIGINFO) == SA_SIGINFO) { // flag SA_SIGINFO was chosen
        sa.sa_sigaction = handler;
    } else {
        sa.sa_handler = handler;
    }

    if ( sigaction(sig_no, &sa, &oldsa) == -1 ) {
        print_error_exit(exec_name, "Can't set sighandler!", proc_id);
        pids_list[proc_id + PROC_COUNT] = 0;
    } else {
        pids_list[proc_id + PROC_COUNT] = 1;
    }

#ifdef DEBUG_HANDS
    printf("%d will receive a signal %d\n", proc_id, sig_no);
    fflush(stdout);
#endif

}   /*  set_sig_handler  */


void forker(int curr_number, int childs_count) {
    pid_t pid = 0;
    int i = 0;

    for (i = 0; i < childs_count; ++i) {

        int chld_id = CHILDS_IDS[curr_number][i];
        if ( (pid = fork() ) == -1) {

            print_error_exit(exec_name, "Can't fork!", chld_id);

        } else if (pid == 0) {  /*  child    */
            proc_id = chld_id;

            if (CHILDS_COUNT[proc_id] != 0) {
                forker(proc_id, CHILDS_COUNT[proc_id]);         // fork children
            }

            break;

        } else {    // pid != 0 (=> parent)
            static int prev_chld_grp = 0;

            int grp_type = GROUP_TYPE[chld_id];

            if (grp_type == 0) {
                if (setpgid(pid, pid) == -1) {
                    print_error_exit(exec_name, "Can't set group", chld_id);
                } else {
                    prev_chld_grp = pid;
                }

            } else if (grp_type == 1) {
                if (setpgid(pid, getpgid(0)) == -1) {
                    print_error_exit(exec_name, "Can't set group", chld_id);
                }
            } else if (grp_type == 2) {
                if (setpgid(pid, prev_chld_grp) == -1) {
                    print_error_exit(exec_name, "Can't set group", chld_id);
                }
            }

        }   // parent branch

    }   // for

}   /*  forker  */
