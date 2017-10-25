/*######################################################*/
/*                                                      */
/*Guner Acet 121044049                                  */
/*                                                      */
/*System Programming final      mylib header file       */
/*                                                      */
/*                                                      */
/*######################################################*/
#include <sys/sem.h>
#include <errno.h>
#include <sys/time.h>
#ifndef MYLIB_H_INCL
#define MYLIB_H_INCL


/*permission*/
#define PERMS (S_IRUSR | S_IWUSR)

/*key path*/
#define   "./"

/*word array size*/
#define WORD_MAX 4096

/*named semaphore*/
#define MYSEMAPHORE "/heysemmy0011"

/*named semaphore 2*/
#define MYSEMAPHORE2 "/hersemmy0022"

/*fifo name*/
#define MYFIFO "myfifo0011"

/*fifo for selling words*/
#define MYFIFO2 "myfifo0012"

/*fifo for paying golds*/
#define MYFIFO3 "myfifo0013"

/*mine log file name*/
#define MYMINELOG "myminelog.txt"

/*million*/
#define MILLION 1000000L



#define READ_FLAGS O_RDONLY
#define WRITE_FLAGS (O_WRONLY | O_CREAT | O_EXCL )
#define WRITE_PERMS (S_IRUSR | S_IWUSR)

typedef struct
{
    char entity[WORD_MAX];
}entity_t;

/*this type will use in different word array*/
typedef struct
{
    int numberofword;/*number of word*/
    int rare;/*if -1 not checking*/
    char word[WORD_MAX];/*word*/
}word_t;

/*file list type*/
typedef struct 
{
    pthread_t id;/*thread id*/
    struct timeval tpstart;/*thread start time*/
    struct timeval tpend;/*thread end time*/
    char path[WORD_MAX];/*file path*/
}fileList_t;


/*fifo data*/
typedef struct
{
	int operation;/*1-selling word,2-request for entity,3-inform directory,4-working*/
	pid_t processid;/*if op==2 -> process id for pending*/
	int entity;/*if op == 1 -> word occurences,if op ==2,entity*/
	char path[WORD_MAX];/*if op == 1,path is word,if op == 3 path directory */
}fifodat_t;

/*sell word*/
typedef struct
{
	int *size;
	int *capacity;
	word_t **array;
	char path[WORD_MAX];
}sell_t;


typedef struct
{
	pid_t id;
	int entity;
}pending_t;

typedef struct
{
	pid_t id;
	struct timeval tpstart;/*work start time*/
    struct timeval tpend;/*served time*/
    int status;/*1-working,2-pending,3-served*/
    int gold;/*paid gold*/
}miner_t;


/**
 *setsembuf function set given buf struct
 *@param sembuf sembuf struct pointer
 *@param num semephore number
 *@param op semaphore operation
 *@param flg semaphore flag
*/
extern void setsembuf(struct sembuf *s, int num, int op, int flg);

/**
 *initelement function initialize semaphore with given id
 *@param semid semaphore id
 *@param semnum semephore number
 *@param semvalue semaphore value for initializing
 *@return if successfull return 0 otherwise return -1
*/
extern int initelement(int semid, int semnum, int semvalue);

/**
 *removesem function remove semaphore with given id
 *@param semid semaphore id
 *@return if successfull return 0 otherwise return -1
*/
extern int removesem(int semid);


/*end of mylib header file*/
#endif 
