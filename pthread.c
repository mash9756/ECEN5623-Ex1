
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


// POSIX thread declarations and scheduling attributes
//
pthread_t threads[2];
threadParams_t threadParams[2];

pthread_attr_t main_attr;
struct sched_param main_param;
pid_t main_pid;

pthread_attr_t inc_attr;
struct sched_param inc_param;

pthread_attr_t dec_attr;
struct sched_param dec_param;

int rt_max_prio, rt_min_prio;

// Unsafe global
int gsum = 0;
int numberOfProcessors;

void print_scheduler(void)
{
   int schedType;
   schedType = sched_getscheduler(getpid());

   switch(schedType)
   {
     case SCHED_FIFO:
           printf("Pthread Policy is SCHED_FIFO\n");
           break;
     case SCHED_OTHER:
           printf("Pthread Policy is SCHED_OTHER\n");
       break;
     case SCHED_RR:
           printf("Pthread Policy is SCHED_OTHER\n");
           break;
     default:
       printf("Pthread Policy is UNKNOWN\n");
   }
}

void *incThread(void *threadp)
{
    int i;
    threadParams_t *threadParams = (threadParams_t *)threadp;

    for(i=0; i<COUNT; i++)
    {
        gsum=gsum+i;
        printf("Increment thread idx=%d, gsum=%d\n", threadParams->threadIdx, gsum);
    }
}

void *decThread(void *threadp)
{
    int i;
    threadParams_t *threadParams = (threadParams_t *)threadp;

    for(i=0; i<COUNT; i++)
    {
        gsum=gsum-i;
        printf("Decrement thread idx=%d, gsum=%d\n", threadParams->threadIdx, gsum);
    }
}

int main (int argc, char *argv[])
{
    int rc      = 0;
    int i       = 0;
    int scope   = 0;
    int coreid  = 0;

    cpu_set_t allcpuset;
    cpu_set_t threadcpu;

    printf("This system has %d processors configured and %d processors available.\n", get_nprocs_conf(), get_nprocs());

    numberOfProcessors = get_nprocs_conf(); 
    printf("number of CPU cores=%d\n", numberOfProcessors);

    CPU_ZERO(&allcpuset);

    for(i=0; i < numberOfProcessors; i++)
        CPU_SET(i, &allcpuset);
    i = 0;
    main_pid = getpid();

    numberOfProcessors = NUM_CPUS;
    printf("Using DEFAULT number of CPUS = %d\n", numberOfProcessors);

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
        printf("PTHREAD SCOPE SYSTEM\n");
    }
    else if (scope == PTHREAD_SCOPE_PROCESS) {
        printf("PTHREAD SCOPE PROCESS\n");
    }
    else {
        printf("PTHREAD SCOPE UNKNOWN\n");
    }

    printf("rt_max_prio=%d\n", rt_max_prio);
    printf("rt_min_prio=%d\n", rt_min_prio);

    CPU_ZERO(&threadcpu);
    printf("Setting thread %d to core %d\n", i, coreid);
    CPU_SET(coreid, &threadcpu);
    if(CPU_ISSET(coreid, &threadcpu)) {
        printf(" CPU-%d ", 0);
    }
    printf("\nLaunching thread %d\n", i);

    rc=pthread_attr_init(&inc_attr);
    rc=pthread_attr_setinheritsched(&inc_attr, PTHREAD_EXPLICIT_SCHED);
    rc=pthread_attr_setschedpolicy(&inc_attr, SCHED_FIFO);
    rc=pthread_attr_setaffinity_np(&inc_attr, sizeof(cpu_set_t), &threadcpu);

    inc_param.sched_priority=rt_max_prio-i-1;
    pthread_attr_setschedparam(&inc_attr, &inc_param);

    threadParams[i].threadIdx=i;
    pthread_create(&threads[i],   // pointer to thread descriptor
                    &inc_attr,     // use default attributes
                    incThread, // thread function entry point
                    (void *)&(threadParams[i]) // parameters to pass in
                    );
    i++;

    CPU_ZERO(&threadcpu);
    printf("Setting thread %d to core %d\n", i, coreid);
    CPU_SET(coreid, &threadcpu);
    if(CPU_ISSET(coreid, &threadcpu)) {
        printf(" CPU-%d ", 0);
    }
    printf("\nLaunching thread %d\n", i);

    rc=pthread_attr_init(&dec_attr);
    rc=pthread_attr_setinheritsched(&dec_attr, PTHREAD_EXPLICIT_SCHED);
    rc=pthread_attr_setschedpolicy(&dec_attr, SCHED_FIFO);
    rc=pthread_attr_setaffinity_np(&dec_attr, sizeof(cpu_set_t), &threadcpu);

    dec_param.sched_priority=rt_max_prio-i-1;
    pthread_attr_setschedparam(&dec_attr, &dec_param);

    threadParams[i].threadIdx=i;
    pthread_create(&threads[i], &dec_attr, decThread, (void *)&(threadParams[i]));

    for(i=0; i<2; i++)
        pthread_join(threads[i], NULL);

    printf("TEST COMPLETE\n");
}
