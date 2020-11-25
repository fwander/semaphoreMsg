#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <semaphore.h>

#define RANGE 1
#define ALLDONE 2

/* gcc -o mutexthr mutexthr.c -lpthread */

typedef struct{
    int iSender; /* sender of the message (0 .. number-of-threads) */
    int type; /* its type */
    int value1; /* first value */
    int value2; /* second value */
} msg;




msg* mailboxes;
sem_t* sems;
sem_t* wsems;   //You can just use a single write semaphore for this project, but I want this to work in the general case.
int numThreads;
int size;
pthread_t* threads;

void SendMsg(int iTo, msg* pMsg){
  int buffer;
  sem_getvalue(wsems + iTo, &buffer);
  sem_wait(wsems + iTo);

  memcpy((mailboxes + iTo), pMsg, sizeof(msg));
  sem_post(sems + iTo);


}
void RecvMsg(int iFrom, msg* pMsg){
  sem_wait(sems + iFrom);
  memcpy(pMsg, (mailboxes + iFrom), sizeof(msg));
  sem_post(wsems + iFrom);
}



int main(int argc, char** argv)
{
    numThreads = atoi(*(argv + 1)) + 1;   // + 1 because of the extra coordinator process.
    size = atoi(*(argv + 2));
    threads = (pthread_t*)malloc((numThreads-1) * sizeof(pthread_t));
    pthread_t parent;
    mailboxes = (msg*)malloc(numThreads * sizeof(msg));
    sems = (sem_t*)malloc(numThreads * sizeof(sem_t));
    wsems = (sem_t*)malloc(numThreads * sizeof(sem_t));
    void* MyThread(void *);
    void* Parent(void *);
    int* args = (int*)malloc(numThreads * sizeof(int));
    for(int i = 0; i < numThreads; i++){
      if(sem_init((sems + i), 0, 0) < 0){
        perror("sems_create");
        exit(1);
      }
      if(sem_init((wsems + i), 0, 1) < 0){
        perror("sems_create");
        exit(1);
      }
    }
    if (pthread_create(&parent, NULL, Parent, (void*)NULL) != 0) {   //coordinator thread
       perror("pthread_create");
       exit(1);
    }
    for (int i = 0; i < numThreads-1; i++){
      *(args + i) = i+1;
      if (pthread_create((threads + i), NULL, MyThread, &args[i]) != 0) {
  	     perror("pthread_create");
  	     exit(1);
      }

    }


    pthread_join(parent,NULL);
    free(mailboxes);
    free(sems);
    free(wsems);
    free(threads);
    free(args);



}

void* Parent(void* arg){
  msg* empty = (msg*)malloc(sizeof(msg));

  int range = size/ (numThreads-1);

  for(int i = 1; i < numThreads; i++){
    empty->type = RANGE;
    empty->value1 = range * (i-1) + 1;
    empty->value2 = i == numThreads-1? size : range * (i-1) + range;
    empty->iSender = 0;

    SendMsg(i,empty);

  }

  int numprocs = 0;
  int total = 0;
  while(numprocs < numThreads - 1){

    RecvMsg(0, empty);
    total += empty->value1;

    numprocs++;
  }
  printf("%d\n", total);
  for (int i = 0 ; i < numThreads-1; i++){
    (void)pthread_join(*(threads + i), NULL);
    (void)sem_destroy((sems + i));
    (void)sem_destroy((wsems + i));
  }
  free(empty);
}

void* MyThread(void* arg)
{
    int sbName;

    sbName = *((int*)arg);
    msg* myMail = (msg*)malloc(sizeof(msg));

    RecvMsg(sbName, myMail);

    int total = 0;
    for(int i = myMail->value1; i <= myMail->value2; i++){
      total += i;
    }
    myMail->type = ALLDONE;
    myMail->value1 = total;
    myMail->iSender = sbName;
    SendMsg(0, myMail);
    free(myMail);
}
