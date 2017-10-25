/*######################################################*/
/*                                                      */
/*Guner Acet 121044049                                  */
/*                                                      */
/*System Programming final      mylib source file       */
/*                                                      */
/*                                                      */
/*######################################################*/
#include "mylib.h"
#ifndef MYLIB_C_INCL
#define MYLIB_C_INCL
/* source = from page 610 in course book pdf */
void setsembuf(struct sembuf *s, int num, int op, int flg)
{
    s->sem_num = (short)num;
    s->sem_op = (short)op;
    s->sem_flg = (short)flg;
    return;
}

/* source = from page 607 in course book pdf */
int initelement(int semid, int semnum, int semvalue) {
	union semun {
		int val;
		struct semid_ds *buf;
		unsigned short *array;
	} arg;
	arg.val = semvalue;
	return semctl(semid, semnum, SETVAL, arg);
}

/* source = from page 608 in course book pdf */
int removesem(int semid) {
	return semctl(semid, 0, IPC_RMID);
}

/*end of mylib source file*/
#endif