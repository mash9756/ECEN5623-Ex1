/**
 *  incdecthread - SCHED_FIFO
 * 
 *  @brief  This is a short program that creates 2 threads,
 *          scheduled via SCHED_FIFO using POSIX pthread functions.
 *          The incThread runs to completion first, then the decThread runs
 *  
 *  @author Mark Sherman and Alexander Bork
 *  @date   02/10/2024
 *  
*/

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

#define NSEC_PER_SEC        (1000000000)
#define NSEC_PER_MSEC       (1000000)
#define NSEC_PER_MICROSEC   (1000)

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

/**
 *  @name   print_scheduler
 *  @brief  prints the current POSIX scheduling policy
 * 
 *  @param  VOID
 * 
 *  @return VOID
*/
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

/**
 *  @name   incThread
 *  @brief  increment thread function that sums over 1000 iterations
 * 
 *  @param  threadp thread parameters, just a thread ID
 * 
 *  @return VOID
*/
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

/**
 *  @name   decThread
 *  @brief  decrement thread function that subtracts over 1000 iterations
 * 
 *  @param  threadp thread parameters, just a thread ID
 * 
 *  @return VOID
*/
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

/**
 *  @name   set_main_sched
 *  @brief  configure main thread scheduling policy and parameters
 *          sets the scheduling policy to SCHED_FIFO
 *          sets thread priority to MAX (99)
 * 
 *  @param  VOID
 * 
 *  @return VOID
*/
void set_main_sched(void) {
    int rc      = 0;
    int scope   = 0;
    main_pid    = getpid();

/**
 *  int sched_get_priority_max(int __algorithm)
 *  int sched_get_priority_min(int __algorithm)
 *      return max or min priority level for the passed scheduling policy
*/
    rt_max_prio = sched_get_priority_max(SCHED_FIFO);
    rt_min_prio = sched_get_priority_min(SCHED_FIFO);

    print_scheduler();
/**
 *  int sched_getparam(pid_t __pid, struct sched_param *__param)
 *      retrieves the current scheduling parameters of the given process
*/
    rc = sched_getparam(main_pid, &main_param);
    main_param.sched_priority = rt_max_prio;
/**
 *  sched_setscheduler(pid_t __pid, int __policy, const struct sched_param *__param)
 *      POSIX function to setup thread scheduling policy
 *      SCHED_FIFO policy runs threads in a first-in first-out order based on priority   
*/
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

/**
 *  @name   set_inc_sched
 *  @brief  configure increment thread scheduling policy and parameters
 *          sets the scheduling policy to SCHED_FIFO
 *          sets thread priority to 98
 * 
 *  @param  VOID
 * 
 *  @return VOID
*/
void set_inc_sched(void) {
    int coreid  = 0;
    cpu_set_t threadcpu;

    CPU_ZERO(&threadcpu);
    CPU_SET(coreid, &threadcpu);
    printf("\nIncrement thread set to run on core %d", coreid);

/**
 *  int pthread_attr_init(pthread_attr_t *__attr)
 *       Initialize thread attribute with default values
 *       (detachstate is PTHREAD_JOINABLE, scheduling policy is SCHED_OTHER, no user-provided stack).
*/
    pthread_attr_init(&inc_attr);
/**
 *  int pthread_attr_setinheritsched(pthread_attr_t *__attr, int __inherit)
 *      set thread attribute inheritance, in this case we want to override
*/
    pthread_attr_setinheritsched(&inc_attr, PTHREAD_EXPLICIT_SCHED);
/**
 *  int pthread_attr_setschedpolicy(pthread_attr_t *__attr, int __policy)
 *      Set scheduling policy within passed thread attribute
*/
    pthread_attr_setschedpolicy(&inc_attr, SCHED_FIFO);
/**
 *  int pthread_attr_setaffinity_np(pthread_attr_t *__attr, size_t __cpusetsize, const cpu_set_t *__cpuset)
 *      Thread created with passed thread attribute will be limited 
 *      to run only on the processors passed
*/
    pthread_attr_setaffinity_np(&inc_attr, sizeof(cpu_set_t), &threadcpu);

    inc_param.sched_priority = rt_max_prio - 1;
    inc_thread_params.threadIdx = 0;
/**
 *  int pthread_attr_setschedparam(pthread_attr_t *__restrict__ __attr, const struct sched_param *__restrict__ __param)
 *      Set scheduling parameters in thread attribute according to given thread parameters.
*/
    pthread_attr_setschedparam(&inc_attr, &inc_param);
}

/**
 *  @name   set_dec_sched
 *  @brief  configure decrement thread scheduling policy and parameters
 *          sets the scheduling policy to SCHED_FIFO
 *          sets thread priority to 97
 * 
 *  @param  VOID
 * 
 *  @return VOID
*/
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
    dec_thread_params.threadIdx = 1;
    pthread_attr_setschedparam(&dec_attr, &dec_param);
}

/**
 *  @name   main
 *  @brief  setup scheduling and launch all threads then wait for completion
*/
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
    set_dec_sched();
/**
 *  int pthread_create( pthread_t *__restrict__ __newthread, 
 *                      const pthread_attr_t *__restrict__ __attr, 
 *                      void *(*__start_routine)(void *), 
 *                      void *__restrict__ __arg)
 * 
 *      Create a new thread running the given function with the passed parameters that runs
 *      with the passed thread attributes
*/
    pthread_create(&inc_thread, &inc_attr, incThread, (void *)&inc_thread_params);
    pthread_create(&dec_thread, &dec_attr, decThread, (void *)&dec_thread_params);

/* wait for threads to complete */
    pthread_join(inc_thread, NULL);
    pthread_join(dec_thread, NULL);
    
    printf("\n\nTEST COMPLETE\n");
}
