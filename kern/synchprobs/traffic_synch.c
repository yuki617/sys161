#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>
#include <array.h>


int MAX_THREADS = 10;
unsigned int thread_num = 0;

/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */



typedef struct Car
{
  Direction origin;
} Car;



static struct lock *intersection_lock;
struct cv *intersection_cv[4];
int *intersection_block[4];
unsigned int go = 0;
struct array *block;

/* 
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 * 
 */
void
intersection_sync_init(void)
{
  /* replace this default implementation with your own implementation */
  intersection_lock = lock_create("intersection_lock");
  if (intersection_lock == NULL) {
    panic("could not create intersection semaphore");
  }
  for(int i=0;i<4;i++) { 
      intersection_cv[i] = cv_create("intersection cv");
      intersection_block[i] = 0;
      if (intersection_cv[i]== NULL) {
	        panic("could not create condition variable");
      }
  }
  block = array_create();
  array_init(block);
  return;
}

/* 
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
void
intersection_sync_cleanup(void)
{
  /* replace this default implementation with your own implementation */
  KASSERT(intersection_lock != NULL);
  lock_destroy(intersection_lock);
  for(int i=0;i<4;i++) { 
    KASSERT(intersection_cv[i] != NULL);
      cv_destroy(intersection_cv[i]);    
  }
  array_destroy(block);
}


/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread 
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */
unsigned int any = 0;

void
intersection_before_entry(Direction origin, Direction destination) 
{
  (void)destination;
  lock_acquire(intersection_lock);
  if(any==0){
    go = origin;
    any=1;
  }
  if(thread_num>3){
      go =-1;
  }
  KASSERT(intersection_lock != NULL);
  if(origin!=go){
    Car *v;
    int change = 1;
    for(unsigned int i=0; i < array_num(block); i++){
        v = array_get(block,i);
        if(v->origin==origin){
          change=0;
          break;
        }
    }
    if(change==1){
      Car *v = kmalloc(sizeof(struct Car));
      v->origin = origin;
      array_add(block,v, NULL);
    }
    intersection_block[origin]++;
    cv_wait(intersection_cv[origin],intersection_lock); 
  }
  thread_num++;
  lock_release(intersection_lock);
}


/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void
intersection_after_exit(Direction origin, Direction destination) {
  (void)origin;
  (void)destination;
  lock_acquire(intersection_lock);
  thread_num--;
  if(thread_num==0){
    intersection_block[origin] =0;
    if(array_num(block)!=0){
      Car *v = array_get(block,0);
      go = v->origin;
      array_remove(block,0);
      cv_broadcast(intersection_cv[go], intersection_lock);
    }else{
      any = 0;
    }
  }
  lock_release(intersection_lock);
}
