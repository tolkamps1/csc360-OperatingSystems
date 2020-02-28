#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>


#define PHILNUMBER 5


typedef struct philosopher{
    int position;
    sem_t *leftFork;
    sem_t *rightFork;
} philosopher_t;



/* Returns random number between max and min
 * for determining sleep time
 */
int get_random_number(int max, int min)
{
  int result = (rand() % (max + 1));
  if (result < min) result = min;
  return result;
}


/**
 * Get forks by locking semaphore if available
 * Resource hierarchy is determined by the fork position in array with
 * Zero index is at the top of the hierarchy IE. all philosophers take 
 * their left fork first except the phil in position 4
 */
void get_forks(philosopher_t *philosopher){
    // Phil 4 takes right fork first
    if(philosopher->position == (PHILNUMBER-1)){
        sem_wait(philosopher->rightFork);
        sem_wait(philosopher->leftFork);
    }
    // All other Phils take left fork first
    else{
        sem_wait(philosopher->leftFork);
        sem_wait(philosopher->rightFork);
    }
}


/**
 * Release forks by unlocking semaphore
 */
void release_forks(philosopher_t *philosopher){
    sem_post(philosopher->rightFork);
    sem_post(philosopher->leftFork);
}



/* Philosopher eats: sleep
 */
void eat(philosopher_t *philosopher){
    // Sleep for a random amount of time
    sleep(get_random_number(5,1));
}


/* Philosopher thinks: sleep
 */
void think(philosopher_t *philosopher){
    // Sleep for a random amount of time
    sleep(get_random_number(5,1));
}


void *philosopher_routine(void *arg){
    philosopher_t *philosopher = (philosopher_t *)arg;
    while(1){
        think(philosopher);
        get_forks(philosopher);
        eat(philosopher);
        release_forks(philosopher);
    }

}


void start_philosophers(pthread_t *threads, sem_t *forks){
    for(int k = 0; k < PHILNUMBER; k++){
        philosopher_t* philosopher = malloc(sizeof(philosopher_t));
        philosopher->position = k;
        philosopher->leftFork = &forks[k];
        philosopher->rightFork = &forks[(k+1) % PHILNUMBER];
        pthread_create(&threads[k], NULL, philosopher_routine, (void *)philosopher);
    }
}


/**
 * Initialize threads and forks
 */
int main (){
    sem_t forks[PHILNUMBER];
    pthread_t threads[PHILNUMBER];
    // Create forks
    for (int k = 0; k < PHILNUMBER; k++){
        sem_init(&forks[k], 0, 1);
    }
    // Create threads
    start_philosophers(threads, forks);
    pthread_exit(NULL);
}