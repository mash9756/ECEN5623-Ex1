
/**
 *  @brief  CU Boulder ECEN 5623 Exercise 1, Part 4(d)
 *          code referenced from Example Report
 * 
 *  @author Mark Sherman and Alexander Bork
 *  @date   02/09/2024
 * 
*/

#define _GNU_SOURCE
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>
#include <time.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdbool.h>

#include <sys/types.h>
#include <sys/sysinfo.h>
#include <unistd.h>

#define NUM_CPUS                (1)
#define FIB_LIMIT_FOR_32_BIT    (47)

#define FIB10_ITERATIONS        (6250000)
#define FIB20_ITERATIONS        (11000000)

#define NSEC_PER_SEC            (1000000000)
#define NSEC_PER_MSEC           (1000000)
#define NSEC_PER_MICROSEC       (1000)

#define MSEC_PER_SEC            (1000)

unsigned int idx = 0, jdx = 1;
unsigned int seqIterations = FIB_LIMIT_FOR_32_BIT;
volatile unsigned int fib = 0, fib0 = 0, fib1 = 1;

#define FIB_TEST(seqCnt, iterCnt)                 \
    for(idx = 0; idx < iterCnt; idx++) {          \
        fib = fib0 + fib1;                        \
        while(jdx < seqCnt)                       \
        {                                         \
            fib0  = fib1;                         \
            fib1  = fib;                          \
            fib   = fib0 + fib1;                  \
            jdx++;                                \
        }                                         \
    } 

typedef struct
{
  int threadIdx;
} threadParams_t;

/* Main thread declarations and sched attributes */
pthread_attr_t main_attr;
struct sched_param main_param;
pid_t main_pid;

/* Fib10 thread declarations and sched attributes */
pthread_t fib10_thread;
threadParams_t fib10_thread_params;
pthread_attr_t fib10_attr;
struct sched_param fib10_param;

/* Fib10 timing declarations */
struct timespec fib10_start   = {0, 0};
struct timespec fib10_finish  = {0, 0};
struct timespec fib10_dt      = {0, 0};

/* Fib20 thread declarations and sched attributes */
pthread_t fib20_thread;
threadParams_t fib20_thread_params;
pthread_attr_t fib20_attr;
struct sched_param fib20_param;

/* Fib20 timing declarations */
struct timespec fib20_start   = {0, 0};
struct timespec fib20_finish  = {0, 0};
struct timespec fib20_dt      = {0, 0};

/* Semaphore declarations */
sem_t fib10_sema;
sem_t fib20_sema;

/* LCM timing declarations */
struct timespec start   = {0, 0};
struct timespec finish  = {0, 0};
struct timespec dt      = {0, 0};

/* Misc globals for SCHED_FIFO and sequencing */
int rt_max_prio = 0;
int rt_min_prio = 0;
bool run_fib10  = true;
bool run_fib20  = true;

void print_scheduler(void)
{
   int schedType;
   schedType = sched_getscheduler(getpid());

   switch(schedType)
   {
     case SCHED_FIFO:
           printf("\nPthread Policy is SCHED_FIFO");
           break;
     case SCHED_OTHER:
           printf("\nPthread Policy is SCHED_OTHER");
       break;
     case SCHED_RR:
           printf("\nPthread Policy is SCHED_OTHER");
           break;
     default:
       printf("\nPthread Policy is UNKNOWN");
   }
}

int delta_t(struct timespec *stop, struct timespec *start, struct timespec *delta_t)
{
  int dt_sec=stop->tv_sec - start->tv_sec;
  int dt_nsec=stop->tv_nsec - start->tv_nsec;

  if(dt_sec >= 0)
  {
    if(dt_nsec >= 0)
    {
      delta_t->tv_sec=dt_sec;
      delta_t->tv_nsec=dt_nsec;
    }
    else
    {
      delta_t->tv_sec=dt_sec-1;
      delta_t->tv_nsec=NSEC_PER_SEC+dt_nsec;
    }
  }
  else
  {
    if(dt_nsec >= 0)
    {
      delta_t->tv_sec=dt_sec;
      delta_t->tv_nsec=dt_nsec;
    }
    else
    {
      delta_t->tv_sec=dt_sec-1;
      delta_t->tv_nsec=NSEC_PER_SEC+dt_nsec;
    }
  }

  return(1);
}

int sleep_ms(int ms) {
    struct timespec req_time;
    struct timespec rem_time;

    int sec   = ms / MSEC_PER_SEC;
    int nsec  = (ms % MSEC_PER_SEC) * NSEC_PER_MSEC;

    req_time.tv_sec   = sec;
    req_time.tv_nsec  = nsec;

    if(nanosleep(&req_time , &rem_time) < 0) {
        printf("Nano sleep system call failed \n");
        return -1;
    }
}

void *fib10_thread_func(void *threadp)
{
    while(run_fib10) {
        sem_wait(&fib10_sema);
        FIB_TEST(FIB_LIMIT_FOR_32_BIT, FIB10_ITERATIONS);
        clock_gettime(CLOCK_REALTIME, &fib10_finish);

        delta_t(&fib10_finish, &start, &fib10_dt);
        printf("\nfib10 timestamp %ld msec\n", (fib10_dt.tv_nsec / NSEC_PER_MSEC));
    }
}

void *fib20_thread_func(void *threadp)
{
    while(run_fib20) {
        sem_wait(&fib20_sema);
        FIB_TEST(FIB_LIMIT_FOR_32_BIT, FIB20_ITERATIONS);
        clock_gettime(CLOCK_REALTIME, &fib20_finish);

        delta_t(&fib20_finish, &start, &fib20_dt);
        printf("\nfib20 timestamp %ld msec\n", (fib20_dt.tv_nsec / NSEC_PER_MSEC)); 
    }
}

/* configure main thread scheduling policy and parameters */
void set_main_sched(void) {
    int rc      = 0;
    int scope   = 0;
    main_pid    = getpid();

    rt_max_prio = sched_get_priority_max(SCHED_FIFO);
    rt_min_prio = sched_get_priority_min(SCHED_FIFO);

    print_scheduler();
    rc = sched_getparam(main_pid, &main_param);
    rc = sched_setscheduler(getpid(), SCHED_OTHER, &main_param);
    if(rc < 0) {
        perror("main_param");
    }
    print_scheduler();

    pthread_attr_getscope(&main_attr, &scope);

    if(scope == PTHREAD_SCOPE_SYSTEM) {
        printf("\nPTHREAD SCOPE SYSTEM");
    }
    else if (scope == PTHREAD_SCOPE_PROCESS) {
        printf("\nPTHREAD SCOPE PROCESS");
    }
    else {
        printf("\nPTHREAD SCOPE UNKNOWN");
    }

    pthread_attr_setschedparam(&main_attr, &main_param);
}

/* configure Fib10 thread scheduling policy and parameters */
void set_fib10_sched(void) {
    int coreid  = 0;
    cpu_set_t threadcpu;

    CPU_ZERO(&threadcpu);
    CPU_SET(coreid, &threadcpu);
    printf("\nfib10 thread set to run on core %d", coreid);

    pthread_attr_init(&fib10_attr);
    pthread_attr_setinheritsched(&fib10_attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&fib10_attr, SCHED_FIFO);
    pthread_attr_setaffinity_np(&fib10_attr, sizeof(cpu_set_t), &threadcpu);

    fib10_param.sched_priority = rt_max_prio - 1;
    fib10_thread_params.threadIdx = 0;
    pthread_attr_setschedparam(&fib10_attr, &fib10_param);
}

/* configure Fib20 thread scheduling policy and parameters */
void set_fib20_sched(void) {
    int coreid  = 0;
    cpu_set_t threadcpu;

    CPU_ZERO(&threadcpu);
    CPU_SET(coreid, &threadcpu);
    printf("\nfib20 thread set to run on core %d\n", coreid);

    pthread_attr_init(&fib20_attr);
    pthread_attr_setinheritsched(&fib20_attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&fib20_attr, SCHED_FIFO);
    pthread_attr_setaffinity_np(&fib20_attr, sizeof(cpu_set_t), &threadcpu);

    fib20_param.sched_priority = rt_max_prio - 2;
    fib20_thread_params.threadIdx = 1;
    pthread_attr_setschedparam(&fib20_attr, &fib20_param);
}

int main (int argc, char *argv[]) {
    cpu_set_t allcpuset;
    int num_processors = get_nprocs_conf();

    printf("\nThis system has %d processors configured and %d processors available.", num_processors, get_nprocs());
    CPU_ZERO(&allcpuset);
    for(int i = 0; i < num_processors; i++)
        CPU_SET(i, &allcpuset);
    num_processors = NUM_CPUS;
    printf("\nRunning all threads on %d CPU core(s)", num_processors);

    sem_init(&fib10_sema, 0, 0);
    sem_init(&fib20_sema, 0, 0);

    set_main_sched();
    set_fib10_sched();
    set_fib20_sched();

    pthread_create(&fib10_thread, &fib10_attr, fib10_thread_func, (void *)&fib10_thread_params);
    pthread_create(&fib20_thread, &fib20_attr, fib20_thread_func, (void *)&fib20_thread_params);

/* Critical instant, start both threads */
    clock_gettime(CLOCK_REALTIME, &start);
    sem_post(&fib10_sema);
    sem_post(&fib20_sema);

/* sleep 20ms, fib10 period */
    sleep_ms(20);
/* start fib10 */
    sem_post(&fib10_sema);
/* sleep 20ms, fib10 period */
    sleep_ms(20);
/* start fib10 */
    sem_post(&fib10_sema);
/* sleep 10ms, first fib20 period (20 + 20 + 10 = 50ms)*/
    sleep_ms(10);
/* stop fib20 execution after next run */
    run_fib20 = false;
/* start fib10 */
    sem_post(&fib20_sema);
/* sleep 10ms, rest of fib10 period (10 + 10 = 20ms)*/
    sleep_ms(10);
/* start fib10 */
    sem_post(&fib10_sema);
/* sleep 20ms, fib10 period */
    sleep_ms(20);
/* stop fib10 execution after next run */
    run_fib10 = false;
    sem_post(&fib10_sema);
    sleep_ms(20);

    clock_gettime(CLOCK_REALTIME, &finish);

    pthread_join(fib10_thread, NULL);
    pthread_join(fib20_thread, NULL);

    delta_t(&finish, &start, &dt);
    printf("\nLCM Period ran for %ld msec\n", (dt.tv_nsec / NSEC_PER_MSEC)); 

    sem_destroy(&fib10_sema);
    sem_destroy(&fib20_sema);
    
    printf("\n\nTEST COMPLETE\n");
}