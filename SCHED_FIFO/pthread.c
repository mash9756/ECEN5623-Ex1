
#define _GNU_SOURCE
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>
#include <time.h>

#include <sys/types.h>
#include <sys/sysinfo.h>
#include <unistd.h>

#define COUNT       (1000)
#define NUM_CPUS    (1)

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

    for(i = 0; i < COUNT; i++)
    {
        gsum = gsum + i;
        printf("\nIncrement thread idx = %d, gsum = %d", threadParams->threadIdx, gsum);
    }
}

void *decThread(void *threadp)
{
    int i;
    threadParams_t *threadParams = (threadParams_t *)threadp;

    for(i = 0; i < COUNT; i++)
    {
        gsum = gsum - i;
        printf("\nDecrement thread idx = %d, gsum = %d", threadParams->threadIdx, gsum);
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
    main_param.sched_priority = rt_max_prio;
    rc = sched_setscheduler(getpid(), SCHED_FIFO, &main_param);
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
    pthread_attr_setschedpolicy(&inc_attr, SCHED_FIFO);
    pthread_attr_setaffinity_np(&inc_attr, sizeof(cpu_set_t), &threadcpu);

    inc_param.sched_priority=rt_max_prio - 1;
    pthread_attr_setschedparam(&inc_attr, &inc_param);

    inc_thread_params.threadIdx = 0;
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
    pthread_attr_setschedpolicy(&dec_attr, SCHED_FIFO);
    pthread_attr_setaffinity_np(&dec_attr, sizeof(cpu_set_t), &threadcpu);

    dec_param.sched_priority = rt_max_prio - 2;
    pthread_attr_setschedparam(&dec_attr, &dec_param);

    dec_thread_params.threadIdx = 1;
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

    set_main_sched();

    set_inc_sched();
    pthread_create(&inc_thread, &inc_attr, incThread, (void *)&inc_thread_params);

    set_dec_sched();
    pthread_create(&dec_thread, &dec_attr, decThread, (void *)&dec_thread_params);

/* wait for threads to complete */
    pthread_join(inc_thread, NULL);
    pthread_join(dec_thread, NULL);
    
    printf("\n\nTEST COMPLETE\n");
}
