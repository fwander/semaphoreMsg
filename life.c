#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <semaphore.h>

#define RANGE 1
#define ALLDONE 2 //
#define GO 3      // Start Gen
#define GENDONE 4 // Generation Done
#define STOP 5
/* gcc -o mutexthr mutexthr.c -lpthread */

typedef struct{
    int iSender; /* sender of the message (0 .. number-of-threads) */
    int type; /* its type */
    int value1; /* first value */
    int value2; /* second value */
} msg; //message threads can pass to each other



/* global shared variables */
msg* mailboxes;
sem_t* sems;
sem_t* wsems;
int numThreads;
int Xdim;
int Ydim;
int** board;
int** nextBoard;
int cGen;
int print;
int input;
void SendMsg(int iTo, msg* pMsg){ // thread safe messaging
  sem_wait(wsems + iTo);
  memcpy((mailboxes + iTo), pMsg, sizeof(msg));
  sem_post(sems + iTo);

}
void RecvMsg(int iFrom, msg* pMsg){
  sem_wait(sems + iFrom);
  memcpy(pMsg, (mailboxes + iFrom), sizeof(msg));
  sem_post(wsems + iFrom);
}

int getCell(int i, int j){  // returns the value of the i,jth cell if out of bounds returns 0
  int* row = (i < 0 || i >= Xdim || j < 0 || j >= Ydim) ? NULL : *(board + i);
  return (row ? *(row + j) : 0);
}

void printBoard(){
  for(int x = 0; x < Xdim; x++){
    for(int y = 0; y < Ydim; y++){
      printf("%d ", *(*(board + x) + y));
    }
    printf("\n");
  }
  printf("\n");
}

int doX(int i){  // 0 if safe to continue, 1 if unchanged, -1 if all 0ed out 
  int total = 0;
  int unchanged = 0;
  int zero = 0;
  for (int j = 0; j < Ydim; j++){        // walk down the row
    total = 0;
    for (int x = -1; x <= 1; x++){
      for (int y = -1; y <= 1; y++){
        total += getCell(i + x, j + y);  //includes i,j

      }
    }
    int thisCell = *(*(board + i) + j);
    total -= thisCell;  // so take it out
    *(*(nextBoard + i) + j) = ( (total < 2 || total > 3) ? 0 : ((total == 3) ? 1 : thisCell)); //rules of life
    zero += *(*(nextBoard + i) + j);
    unchanged += *(*(nextBoard + i) + j) != thisCell;
  }

  return !(!zero || !unchanged) ? 0 : zero ? (unchanged ? -1 : 2) : 1;
}

void fillBoard(FILE* file){ //fills board with init file
  char* buf = (char*) malloc(Ydim*2+1);
  for(int i = 0; i < Xdim; i ++){

    fgets(buf, Ydim*2+1, file);

    int j = 0;
    while (j < Ydim){
      //printf("%c", *(buf + j*2));
      board[i][j] = *(buf + j*2) - '0';
      j++;
    }

  }
  free(buf);
}

void setDim(char* str){ // run wc on the input file to set size.
  int len = strlen(str);
  char* command = (char*)malloc(len + 4);
  *command = 'w';
  *(command+1) = 'c';
  *(command+2) = ' ';
  for(int i = 0; i < len + 1; i++) *(command + i + 3) = '\0';
  FILE *wc = popen(strcat(command,str),"r");
  char* buf = (char*) malloc(16);
  fgets(buf, 16, wc);

  Xdim = atoi(strtok(buf, " "));
  Ydim = atoi(strtok(NULL, " "))/Xdim;

  free(buf);
  free(command);
  pclose(wc);
}



int main(int argc, char** argv)
{
    if (argc < 2) {
        printf("use: life-c [number of threads] [generation 0 text file] [number of generations to carry out] (OPTIONAL args ->) [y|n] (print every generation) [y|n] (wait for input after every generation)\n");
        return 0;
    }
    void* MyThread(void *);
    void* Parent(void *);
    numThreads = atoi(*(argv + 1)) + 1;

    cGen = atoi(argv[3]);
    
    if (argc > 4 && *argv[4] == 'y')
      print = 1;

    if (argc > 5 && *argv[5] == 'y')
      input = 1;

    setDim(argv[2]);
    FILE* fp = fopen(argv[2],"r");
    numThreads = (numThreads - 1) > Xdim ? Xdim + 1 : numThreads;

    board = (int**)malloc(Xdim * sizeof(int*));
    nextBoard = (int**)malloc(Xdim * sizeof(int*));
    for(int i = 0; i < Xdim; i++){
      *(board + i) = (int*)malloc(Ydim * sizeof(int));
      *(nextBoard + i) = (int*)malloc(Ydim * sizeof(int));

    }
    fillBoard(fp);
    fclose(fp);


    printBoard();
    fflush(stdout);
    pthread_t* threads = (pthread_t*)malloc((numThreads) * sizeof(pthread_t));
    pthread_t parent;

    mailboxes = (msg*)malloc(numThreads * sizeof(msg));
    sems = (sem_t*)malloc(numThreads * sizeof(sem_t));
    wsems = (sem_t*)malloc(numThreads * sizeof(sem_t));

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
    if (pthread_create(threads, NULL, Parent, (void*)NULL) != 0) {   //coordinator thread
       perror("pthread_create");
       exit(1);
    }
    for (int i = 1; i < numThreads; i++){
      *(args + i) = i;
      if (pthread_create((threads + i), NULL, MyThread, &args[i]) != 0) {
         perror("pthread_create");
         exit(1);
      }

    }

    for (int i = 0; i < numThreads; i++){
      (void)pthread_join(*(threads + i), NULL);
      (void)sem_destroy((sems + i));
      (void)sem_destroy((wsems + i));
    }
    //pthread_join(parent,NULL);
     for(int i = 0; i < Xdim; i++){
        free(*(board + i));
         free(*(nextBoard + i));
     }
     free(board);
     free(nextBoard);
     free(mailboxes);
     free(sems);
     free(wsems);
     free(threads);
     free(args);

}

void* Parent(void* arg){
  msg* empty = (msg*)malloc(sizeof(msg));

  int range = Xdim/ (numThreads-1);
  for(int i = 1; i < numThreads; i++){ //send range to all children threads
    empty->type = RANGE;
    empty->value1 = range * (i-1);
    empty->value2 = i == numThreads-1? Xdim - 1 : range * (i-1) + range - 1;
    empty->iSender = 0;
    SendMsg(i,empty);
  }

  int numprocs = 0;
  int gendone = 0;
  int total = 0;
  int zero = 0;
  int unchanged = 0;
  while(numprocs < cGen){
    empty->type = GO;
    gendone = 0;
    unchanged = 0;
    zero = 0;
    for(int i = 1; i < numThreads; i++){


      SendMsg(i,empty);
    }

    while(gendone < numThreads-1){
      RecvMsg(0, empty);
      gendone += empty->type == GENDONE ? 1 : 0;
      zero += empty->value1;
      unchanged += empty->value2;
    }
    
    for(int i = 0; i < Xdim; i++){
      memcpy(*(board + i), *(nextBoard + i), Ydim * sizeof(int));
    }
    if (zero == Xdim || unchanged == Xdim){
      empty->type = STOP;
      for(int i = 1; i < numThreads; i++){
        SendMsg(i,empty);
      }
      break;
    }
    
    if (print){
       printf("gen%d:\n", numprocs+1);
       printBoard();
    }
    if (input){
      fflush (stdout);
      getchar();
    }
    numprocs++;

  }
  printf("life after %d gens looks like:\n", numprocs);
  printBoard();

  free(empty);

  //printf("%d\n", total);


}

void* MyThread(void* arg)
{
    int sbName;

    sbName = *((int*)arg);
    msg* myMail = (msg*)malloc(sizeof(msg));
    RecvMsg(sbName, myMail);
    int lowerRange = myMail->value1;
    int upperRange = myMail->value2;
    int zero = 0;
    int unchanged = 0;
    int value;
    //printf("Hello from thread %d range: %d - %d\n", sbName, myMail->value1, myMail->value2);
    for(int i = 1; i <= cGen; i++){
      RecvMsg(sbName, myMail);
      if(myMail->type == STOP){
      	break;
      }
      zero = 0;
      unchanged = 0;
      for(int x = lowerRange; x <= upperRange; x++){
	value = doX(x);
        zero += value == 1 || value == 2 ? 1 : 0;
	unchanged += value == -1 || value == 2 ? 1: 0;
      }
      myMail->iSender = sbName;
      myMail->type = GENDONE;
      myMail->value1 = zero;
      myMail->value2 = unchanged;
      SendMsg(0,myMail);
    }

    myMail->type = ALLDONE;

    myMail->iSender = sbName;
    free(myMail);
    //SendMsg(0, myMail);
}
