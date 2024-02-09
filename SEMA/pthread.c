
#define _GNU_SOURCE
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>
#include <time.h>
#include <semaphore.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/sysinfo.h>
#include <unistd.h>

#define COUNT       (1000)
#define NUM_CPUS    (1)
#define NUM_THREADS (2)

#define NSEC_PER_SEC (1000000000)
#define NSEC_PER_MSEC (1000000)
#define NSEC_PER_MICROSEC (1000)

typedef struct
{
    int threadIdx;
} threadParams_t;

/* Main thread declarations and sched attributes */
pthread_attr_t main_attr;
struct sched_param main_param;
pid_t main_pid;

/* Increment thread declarations and sched attributes */
pthread_t inc_thread;
threadParams_t inc_thread_params;
pthread_attr_t inc_attr;
struct sched_param inc_param;

/* Decrement thread declarations and sched attributes */
pthread_t dec_thread;
threadParams_t dec_thread_params;
pthread_attr_t dec_attr;
struct sched_param dec_param;

/** 
 * Semaphore declarations 
 *      increment thread waits for inc_sema to post, then runs
 *      once completed, it posts the dec_sema, which allows the decrement thread to run
 *      main thread sleeps until both threads complete
*/
sem_t inc_sema;
sem_t dec_sema;
uint8_t active_threads = 0;

int rt_max_prio = 0;
int rt_min_prio = 0;
int gsum        = 0;

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

void *incThread(void *threadp)
{
    int i;
    threadParams_t *threadParams = (threadParams_t *)threadp;

    printf("\nInc waiting for semaphore");
    sem_wait(&inc_sema);
    for(i = 0; i < COUNT; i++)
    {
        gsum = gsum + i;
        printf("\nIncrement thread idx = %d, gsum = %d", threadParams->threadIdx, gsum);
    }
    active_threads--;
    printf("\nInc completed");
    sem_post(&dec_sema);
    return NULL;
}

void *decThread(void *threadp)
{
    int i;
    threadParams_t *threadParams = (threadParams_t *)threadp;

    printf("\nDec waiting for semaphore");
    sem_wait(&dec_sema);
    for(i = 0; i < COUNT; i++)
    {
        gsum = gsum - i;
        printf("\nDecrement thread idx = %d, gsum = %d", threadParams->threadIdx, gsum);
    }
    active_threads--;
    printf("\nDec completed");
    return NULL;
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

    printf("\nrt_max_prio = %d", rt_max_prio);
    printf("\nrt_min_prio = %d\n", rt_min_prio);
}

/* configure increment thread scheduling policy and parameters */
void set_inc_sched(void) {
    int coreid  = 0;
    cpu_set_t threadcpu;

    CPU_ZERO(&threadcpu);
    CPU_SET(coreid, &threadcpu);
    printf("\nIncrement thread set to run on core %d", coreid);

    pthread_attr_init(&inc_attr);
    pthread_attr_setinheritsched(&inc_attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&inc_attr, SCHED_OTHER);
    pthread_attr_setaffinity_np(&inc_attr, sizeof(cpu_set_t), &threadcpu);

    inc_thread_params.threadIdx = 0;
    pthread_attr_setschedparam(&inc_attr, &inc_param);
}

/* configure decrement thread scheduling policy and parameters */
void set_dec_sched(void) {
    int coreid  = 0;
    cpu_set_t threadcpu;

    CPU_ZERO(&threadcpu);
    CPU_SET(coreid, &threadcpu);
    printf("\nDecrement thread set to run on core %d\n", coreid);

    pthread_attr_init(&dec_attr);
    pthread_attr_setinheritsched(&dec_attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&dec_attr, SCHED_OTHER);
    pthread_attr_setaffinity_np(&dec_attr, sizeof(cpu_set_t), &threadcpu);

    dec_thread_params.threadIdx = 1;
    pthread_attr_setschedparam(&dec_attr, &dec_param);
}

int main (int argc, char *argv[])
{
    cpu_set_t allcpuset;
    int num_processors = get_nprocs_conf();

    printf("\nThis system has %d processors configured and %d processors available.", num_processors, get_nprocs());
    CPU_ZERO(&allcpuset);
    for(int i = 0; i < num_processors; i++)
        CPU_SET(i, &allcpuset);
    num_processors = NUM_CPUS;
    printf("\nRunning all threads on %d CPU core(s)", num_processors);

    sem_init(&inc_sema, 0, 0);
    sem_init(&dec_sema, 0, 0);
   
    set_main_sched();
    set_inc_sched();
    set_dec_sched();

    pthread_create(&inc_thread, &inc_attr, incThread, (void *)&inc_thread_params);
    active_threads++;

    pthread_create(&dec_thread, &dec_attr, decThread, (void *)&dec_thread_params);
    active_threads++;

    sem_post(&inc_sema);

    pthread_detach(inc_thread);
    pthread_detach(dec_thread);
    
    while(active_threads > 0){
        sleep(1);
    }

    sem_destroy(&inc_sema);
    sem_destroy(&dec_sema);
    
    printf("\n\nTEST COMPLETE\n");
}
