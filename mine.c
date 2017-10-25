/*######################################################*/
/*                                                      */
/*Guner Acet 121044049                                  */
/*                                                      */
/*System Programming final      mine                    */
/*                                                      */
/*                                                      */
/*######################################################*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <string.h>
#include <limits.h>
#include <sys/wait.h>
#include <pthread.h>
#include <sys/time.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <ctype.h>
#include <sys/shm.h>
#include <semaphore.h>
#include "mylib.h"
#include <error.h>
#include <signal.h>
#include <sys/msg.h>
/*fprintf(stderr,"T\nU\nT\nT\nU\nR\nU\nU\n!\n");*/


void *allocateshm(key_t key,sem_t *semaphore,entity_t *array,int size);
void getEntity(DIR *dirp,entity_t **array,int *size,int *capacity,char path[WORD_MAX]);/*get entity*/
int wordSave(word_t input,word_t **array,int *size,int *capacity);/*add given word in given array*/
int reallocSharedMem(void *data);/*reallocate shared memory*/
void addElement(pending_t *array,int *size,int entity,pid_t id);/*add element to given array*/
pending_t getElement(pending_t *array,int *size);/*get min element and fit array*/
int getMinEntity(pending_t *array,int size);/*get min entity*/
void addMiner(miner_t **array,int *size,int *capacity,pid_t id,int status,struct timeval tpstart);/*add miner to list*/
void setMiner(miner_t *array,int size,pid_t id,int status,struct timeval tpend,int gold);/*set miners*/
void logUpdater(void *data);/*log update*/

static int idd=0;
static int idd2=0;
static int fdlog=0;/*log file*/
static int servedMiners=0;/*numberofserved miners*/
static int pendingMiners=0;/*numberof pending miners*/
static int workingMiners=0;/*numberof working miners*/
static miner_t *minerList;/*miner list*/
static struct timeval tpstart;/*mine start time*/
static struct timeval tpend;/*mine end time*/
static int mineStatus=1;/*mine status*/
static word_t *wordList;/*mine word list*/
static int mineSize=0;/*mine word array size*/
static int allCoin=0;/*total mine coin*/
static int a;/*for loop*/
static int iMinerSize=0;/*miner array size*/
static int fd;/*fifo*/
static sem_t *mysem,*mysem2;/*semaphore*/
static key_t mykey,mykey2;/*key*/
static int *s;/*shared mem size*/

void  SEGFAULThandler(int sig)
{
    idd = shmget(mykey,sizeof(int), PERMS);
    s=(int*)shmat(idd,NULL,0);
    idd2 = shmget(mykey2,sizeof(entity_t)*(*s), PERMS);
    shmdt((void *) s);

    if(shmctl(idd, IPC_RMID, NULL)==-1)
        perror("failed to remove shared mem");
    if(shmctl(idd2, IPC_RMID, NULL)==-1)
        perror("failed to remove shared mem2");
    fprintf(stderr,"SIGINT catched\n");
    close(fd);

    if (unlink(MYFIFO) == -1)
    {
        perror("Failed to remove fifo");
    }

    if (unlink(MYFIFO2) == -1)
    {
        perror("Failed to remove fifo2");
    }

    if (unlink(MYFIFO3) == -1)
    {
        perror("Failed to remove fifo3");
    }

    for(a=0;a<iMinerSize;++a)
    {
        if(minerList[a].status!=3)
        {
            kill(minerList[a].id,SIGINT);
        }
    }
    mineStatus=0;
    if (gettimeofday(&tpend, NULL)) /*get starting time*/
    {
        fprintf(stderr, "Failed to get start time\n");
    }
    logUpdater(&iMinerSize);
    if(iMinerSize!=0)
        free(minerList);
    if(mineSize!=0)
        free(wordList);

    sem_close(mysem);
    sem_close(mysem2);
    sem_unlink(MYSEMAPHORE);
    sem_unlink(MYSEMAPHORE2);
    exit(0);
}

void hand(int sig)
{
    /**/
}


int main(int argc,char **argv){
    entity_t *entityArr;/*entity list -> malloc*/
    entity_t *entityArrShared;/*shared entity list*/
    int i;/*increment*/
    int shmid,shmid2;/*share id*/
    DIR *dirp;/*mines*/
    struct dirent *direntp;/*for searching entities*/
    int fd2;/*for fifo 2,selling word*/
    int fd3;/*for fifo 3,paying gold*/
    word_t wordFromMiner;/*miner words*/
    char curPath[WORD_MAX];/*for sarching entities*/
    int *sharedSize;/*shared size*/
    fifodat_t data;/*data from fifo*/

    int status=1;/*read status*/
    int mineCapacity=0;/*mine word array capacity*/
    int size=0;/*entity  array size*/
    int capacity=0;/*entity capacity size*/
    pending_t pendingList[30];/*pending miners*/
    int iPendingSize=0;/*pending array size*/
    pending_t pending;/*temp*/
    int iIsExist;/*word rare*/
    int iMinerCapacity=0;/*miner array cap*/
    struct timeval tptemp;/*miner timer*/
    int totalCoin=0;/*total coin for one miner*/

    signal(SIGSEGV, SEGFAULThandler);     /*install the handler    */
    signal(SIGUSR1, hand);     /*install the handler    */
    signal(SIGINT, SEGFAULThandler);     /*install the handler    */

    if (gettimeofday(&tpstart, NULL)) /*get starting time*/
    {
        fprintf(stderr, "Failed to get start time\n");
    }

    if(argc <2 || argc >20 )
    {
        fprintf(stderr, "illegal parameters,more or less\nUsege: mine -<dir1> -<dir2> ...\n" );
        return 1;
    }

    for(i=1;i<argc;++i)/*open directories*/
    {
        dirp=opendir(&argv[i][1]);
        if(dirp==NULL)
        {
            fprintf(stderr,"illegal parameters, %s not a directory\nUsege: mine -<dir1> -<dir2> ...\n",&argv[i][1]);
            if(size!=0)
                free(entityArr);
            return 1;
        }
        do
        {
            direntp=readdir(dirp);
            if(direntp==NULL)
            {
            break;
            }
            if(strcmp(direntp->d_name,"..")!=0 && strcmp(direntp->d_name,".")!=0 && direntp->d_name[strlen(direntp->d_name)-1]!='~')
            {
                if(size==0)
                {
                    capacity=10;
                    entityArr=(entity_t*)malloc(sizeof(entity_t)*capacity);
                }
                else if(size==capacity)
                {
                    capacity+=10;
                    entityArr=(entity_t*)realloc(entityArr,sizeof(entity_t)*capacity);
                }
                strcpy(curPath,&argv[i][1]);
                strcat(curPath,"/");
                strcat(curPath,direntp->d_name);
                strcpy(entityArr[size].entity,curPath);
                size++;
            }

        }while(direntp!=NULL);
        closedir(dirp);
        
    }

    if(size==0)
    {
        fprintf(stderr, "no found regular entities\n");
        return 1;
    }
    if ((mykey = ftok(KEY_PATH,1)) == (key_t)-1)
    {
        perror("Failed to derive key");
        if(size!=0)
        {
            free(entityArr);
        }
        return 1;
    }

    if ((mykey2 = ftok(KEY_PATH,2)) == (key_t)-1)
    {
        perror("Failed to derive key");
        if(size!=0)
        {
            free(entityArr);
        }
        return 1;
    }

    mysem=sem_open(MYSEMAPHORE,O_CREAT,PERMS,1);
    if(sem_init(mysem,1,1)==-1)/*semaphore init*/
    {
        perror("failed init semaphore\n");
    }

    mysem2=sem_open(MYSEMAPHORE2,O_CREAT,PERMS,1);
    if(sem_init(mysem2,1,1)==-1)/*semaphore init*/
    {
        perror("failed init semaphore\n");
    }

    fprintf(stderr,"%d entities found\n",size);

    /*start of critical section*/
    /*--------------------------------------------------------*/
    if(sem_wait(mysem)==-1)
    {
        perror("semaphore locking");
    }
    if ((shmid = shmget(mykey,sizeof(int), PERMS | IPC_CREAT)) == -1) {
        perror("Failed to create shared memory segment");
        if(size!=0)
        {
            free(entityArr);
        }
        return 1;
    }
    idd=shmid;
    sharedSize=(int*)shmat(shmid,NULL,0);
    if(sharedSize==NULL)
    {
        perror("sharedSize is null");
    }
    if ((shmid2 = shmget(mykey2,sizeof(entity_t)*size, PERMS | IPC_CREAT)) == -1) {
        perror("Failed to create shared memory segment");
        if(size!=0)
        {
            free(entityArr);
        }
        return 1;
    }

    idd2=shmid2;

    entityArrShared=(entity_t*)shmat(shmid2,NULL,0);
    if(entityArrShared==NULL)
    {
        perror("entityArrShared is null");
    }
    *sharedSize=size;
    for(i=0;i<size;++i)
    {
        strcpy(entityArrShared[i].entity,entityArr[i].entity);
    }
    free(entityArr);

    shmdt((void *) entityArrShared);
    shmdt((void *) sharedSize);


    if(sem_post(mysem)==-1)
    {
        perror("semaphore unlocking\n");
    }

    /*------------------------------------------------------------*/
    /*end of critical section*/

    if (mkfifo(MYFIFO,S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) == -1)
    {
        perror("Failed to create myfifo");
    }

    if (mkfifo(MYFIFO2,S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) == -1)
    {
        perror("Failed to create myfifo2");
    }

    if (mkfifo(MYFIFO3,S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) == -1)
    {
        perror("Failed to create myfifo3");
    }

    fd = open(MYFIFO, O_RDONLY);
    if(fd==-1)
    {
        perror("open fifo failed in mine");
    }

    while(1)
    {
        status=read(fd,&data,sizeof(fifodat_t));
        if(status!=0 && status!=-1)
        {
            if(data.operation==1)/*sell word*/
            {
                fprintf(stderr,"miner works done...%d\n",(int)data.processid);
                
                fd3=open(MYFIFO3,O_WRONLY);
                if(fd3==-1)
                {
                    perror("open fifo 3 failed");
                }

                fd2=open(MYFIFO2,O_RDONLY);
                if(fd2==-1)
                {
                    perror("open fifo 2 failed");
                }
                
                while(read(fd2,&wordFromMiner,sizeof(word_t))!=0)
                {
                    iIsExist=wordSave(wordFromMiner,&wordList,&mineSize,&mineCapacity);
                    if(write(fd3,&iIsExist,sizeof(int))==-1)
                    {
                        perror("mine-writing word rare");
                    }
                    if(iIsExist==0)
                    {
                        if(wordFromMiner.numberofword==1)
                        {
                            totalCoin+=10;
                        }
                        else
                        {
                            totalCoin+=10+wordFromMiner.numberofword-1;
                        }
                    }
                    else
                    {
                        totalCoin+=wordFromMiner.numberofword;
                    }
                }
                close(fd2);
                close(fd3);
                servedMiners++;
                workingMiners--;
                if (gettimeofday(&tptemp, NULL)) /*get starting time*/
                {
                    fprintf(stderr, "Failed to get start time\n");
                }
                setMiner(minerList,iMinerSize,data.processid,3,tptemp,totalCoin);
                allCoin+=totalCoin;
                totalCoin=0;
            }
            else if(data.operation==2)/*pending miner*/
            {
                fprintf(stderr,"pending miner...%d\n",(int)data.processid);
                if (gettimeofday(&tptemp, NULL)) /*get starting time*/
                {
                    fprintf(stderr, "Failed to get start time\n");
                }
                addMiner(&minerList,&iMinerSize,&iMinerCapacity,data.processid,2,tptemp);
                addElement(pendingList,&iPendingSize,data.entity,data.processid);
                pendingMiners++;
            }
            else if(data.operation==3)/*inform directory*/
            {
                fprintf(stderr,"found new resource(directory) ...from %d\n",(int)data.processid);
                if(reallocSharedMem(data.path)>=getMinEntity(pendingList,iPendingSize) && pendingMiners>0)
                {
                    pending=getElement(pendingList,&iPendingSize);
                    kill(pending.id,SIGUSR1);
                    if (gettimeofday(&tptemp, NULL)) /*get starting time*/
                    {
                        fprintf(stderr, "Failed to get start time\n");
                    }
                    setMiner(minerList,iMinerSize,pending.id,1,tptemp,0);
                    pendingMiners--;
                    workingMiners++;
                    fprintf(stderr,"pending miner starts working...%d\n",(int)pending.id);
                }
                
            }
            else if(data.operation==4)/*working*/
            {
                fprintf(stderr,"miner starts working...%d\n",(int)data.processid);
                if (gettimeofday(&tptemp, NULL)) /*get starting time*/
                {
                    fprintf(stderr, "Failed to get start time\n");
                }
                addMiner(&minerList,&iMinerSize,&iMinerCapacity,data.processid,1,tptemp);
                workingMiners++;
            }
            else
            {
                fprintf(stderr,"FATALITY ERROR-DATA OPERATION WRONG!!\n");
            }
            logUpdater(&iMinerSize);
        }
    }
    return 0;
}

/**reallocSharedMem function runs if miner find a directory
 *@param data path of directory
 *@return new entity size
 */
int reallocSharedMem(void *data)
{
    key_t mykey,mykey2;/*key*/
    sem_t *mysem;/*semaphore*/
    entity_t *tempArr;/*temp array*/
    int tempArrSize=0;/*size of temp array*/
    int tempCapacity;/*capacity of temp array*/
    int shmid,shmid2;/*shared mem id*/
    int *sharedSize;/*shared array size*/
    entity_t *entityArrShared;/*shared array*/
    DIR *dirp;/*directory ptr*/
    int i;/*increment*/

    if ((mykey = ftok(KEY_PATH, 1)) == (key_t)-1)
    {
        perror("Failed to derive key");
    }

    if ((mykey2 = ftok(KEY_PATH, 2)) == (key_t)-1)
    {
        perror("Failed to derive key");
    }

    mysem=sem_open(MYSEMAPHORE,O_CREAT,PERMS,1);
    /*start of critical section*/
    /*--------------------------------------------------------*/
    if(sem_wait(mysem)==-1)
    {
        perror("semaphore locking");
    }   


    if ((shmid = shmget(mykey,sizeof(int), PERMS)) == -1) {
        perror("Failed to open shared memory segment");
    }
    idd=shmid;
    sharedSize=(int*)shmat(shmid,NULL,0);
    if(sharedSize==NULL)
    {
        perror("sharedSize is null");
    }

    if(*sharedSize>0)
    {
        if ((shmid2 = shmget(mykey2,sizeof(entity_t)*(*sharedSize), PERMS)) == -1)
        {
            perror("Failed to create shared memory segment");
        }
        idd2=shmid2;
        entityArrShared=(entity_t*)shmat(shmid2,NULL,0);
        if(entityArrShared==NULL)
        {
            perror("entityArrShared is null");
        }

        tempArr=(entity_t*)malloc(sizeof(entity_t)*(*sharedSize));

        for(i=0;i<*sharedSize;++i)
        {
            strcpy(tempArr[i].entity,entityArrShared[i].entity);
        }

        shmdt((void *) entityArrShared);
        shmctl(shmid2, IPC_RMID, NULL);
    }
                
    dirp=opendir((char*)data);
    tempArrSize=*sharedSize;
    tempCapacity=*sharedSize;
    getEntity(dirp,&tempArr,&tempArrSize,&tempCapacity,(char*)data);
    closedir(dirp);
    if(tempArrSize>0)
    {
        if ((shmid2 = shmget(mykey2,sizeof(entity_t)*(tempArrSize), PERMS | IPC_CREAT)) == -1)
        {
            perror("Failed to create shared memory segment");
        }
        idd2=shmid2;
        entityArrShared=(entity_t*)shmat(shmid2,NULL,0);
        if(entityArrShared==NULL)
        {
            perror("entityArrShared is null");
        }
    }

    *sharedSize=tempArrSize;
    for(i=0;i<tempArrSize;++i)
    {
        strcpy(entityArrShared[i].entity,tempArr[i].entity);
    }

    shmdt((void *) sharedSize);
    if(tempArrSize!=0)
    {
        shmdt((void *) entityArrShared);
        free(tempArr);
    }
    if(sem_post(mysem)==-1)
    {
        perror("semaphore unlocking\n");
    }

    /*------------------------------------------------------------*/
    /*end of critical section*/

    sem_close(mysem);
    return tempArrSize;

}


void getEntity(DIR *dirp,entity_t **array,int *size,int *capacity,char path[WORD_MAX])
{
    char curPath[WORD_MAX];/*curr entity*/
    struct dirent *direntp;/*for reading path's files and directories names*/
    do
    {
        direntp=readdir(dirp);
        if(direntp==NULL)
        {
            break;
        }
        if(strcmp(direntp->d_name,"..")!=0 && strcmp(direntp->d_name,".")!=0 && direntp->d_name[strlen(direntp->d_name)-1]!='~')
        {
            if(*size==0)
            {
                *capacity=10;
                *array=(entity_t*)malloc(sizeof(entity_t)*(*capacity));
            }
            else if(*size==*capacity)
            {
                *capacity+=10;
                *array=(entity_t*)realloc(*array,sizeof(entity_t)*(*capacity));
            }
            strcpy(curPath,path);
            strcat(curPath,"/");
            strcat(curPath,(*direntp).d_name);

            strcpy((*array)[*size].entity,curPath);
            *size+=1;
        }

    }while(direntp!=NULL);
}


/**
 *saveWord function takes a word if the word exist in array,increase number of the word,
 *otherwise reallocate array and add the word to array
 *@param word char pointer for search or add a word
 *@return 1 if given word exist in arrays,otherwise return 0
 */
int wordSave(word_t input,word_t **array,int *size,int *capacity)
{
    int i;
    if(*size==0)
    {
        *capacity=10;
        *array=(word_t*)malloc(sizeof(word_t)*(*capacity));
    }
    for(i=0;i<*size;++i)
    {
        if(strcmp((*array)[i].word,input.word)==0)
        {
            (*array)[i].numberofword+=input.numberofword;
            return 1;
        }
    }
    if(*size==*capacity)
    {
        *capacity+=20;
        *array=(word_t*)realloc(*array,sizeof(word_t)*(*capacity));
    }
    strcpy((*array)[*size].word,input.word);
    (*array)[*size].numberofword=input.numberofword;
    (*array)[*size].rare=-1;
    *size+=1;
    return 0;
}

/**addElement add given element to given array
 *@param array pending array
 *@param size size of array
 *@param entity amount of needed entity for miner
 *@param id miner's process id
 */
void addElement(pending_t *array,int *size,int entity,pid_t id)
{
    if(*size==0)
    {
        array[0].id=id;
        array[0].entity=entity;
        *size+=1;
        return;
    }
    array[*size].id=id;
    array[*size].entity=entity;
    *size+=1;
    return;
}

/**getElement get min entity element from given array
 *@param array pending array
 *@param size size of array
 *@return return min element
 */
pending_t getElement(pending_t *array,int *size)
{
    int i,index;
    pending_t temp;
    if(*size==1)
        return array[0];
    temp=array[0];
    index=0;
    for(i=1;i<*size;++i)
    {
        if(array[i].entity<temp.entity)
        {
            temp=array[i];
            index=i;
        }
    }
    if(index==*size-1)
    {
        *size-=1;
        return array[index];
    }
    array[index]=array[*size-1];
    *size-=1;
    return temp;
}

/**getMinEntity get element that min entity value,no change array
 *@param array pending list
 *@param size size of list
 *@return return min entity value
 */
int getMinEntity(pending_t *array,int size)
{
    int temp,i;
    if(size<=0)
    {
        return -1;
    }
    else if(size==1)
    {
        return array[0].entity;
    }
    temp=array[0].entity;
    for(i=1;i<size;++i)
    {
        if(array[i].entity < temp)
            temp=array[i].entity;
    }
    return temp;
}

/**addMiner add new miner to given list
 *@param array minerList array
 *@param size minerList size
 *@param capacity array capacity
 *@param id miner process id
 *@param status 1-working,2-pending,3-served
 *@param tpstart miner starting time
 */
void addMiner(miner_t **array,int *size,int *capacity,pid_t id,int status,struct timeval tpstart)
{
    if(*size==0)
    {
        *capacity=10;
        *array=(miner_t*)malloc(sizeof(miner_t)*(*capacity));
    }
    if(*size==*capacity)
    {
        *capacity+=20;
        *array=(miner_t*)realloc(*array,sizeof(miner_t)*(*capacity));
    }
    (*array)[*size].id=id;
    (*array)[*size].status=status;
    (*array)[*size].gold=0;
    (*array)[*size].tpstart=tpstart;
    *size+=1;

    return;
}

/**setMiner set given miner in given list
 *@param array minerList array
 *@param size minerList size
 *@param capacity array capacity
 *@param id miner process id
 *@param status 1-working,2-pending,3-served
 *@param tpend miner ending time
 *@param gold amount of paid gold
 */
void setMiner(miner_t *array,int size,pid_t id,int status,struct timeval tpend,int gold)
{
    int i;
    for(i=0;i<size;++i)
    {
        if(array[i].id==id)
        {
            array[i].status=status;
            if(status==3)
            {
                array[i].gold=gold;
                array[i].tpend=tpend;
            }
            if(status==1)
            {
                array[i].tpstart=tpend;
            }
            return;
        }
    }
    return;
}

/**log file updater
 *@param data size of mineList array
 */
void logUpdater(void *data)
{
    char logname[WORD_MAX];
    int i;
    long timedif;
    int tw=0;/*total words*/
    int size=*(int*)data;
    fdlog=open(MYMINELOG,WRITE_FLAGS,WRITE_PERMS);
    if(fdlog==-1)
    {
        if(errno==9)
        {
            close(fdlog);
            perror("close log file");
        }
        unlink(MYMINELOG);
        fdlog=open(MYMINELOG,WRITE_FLAGS,WRITE_PERMS);
        if(fdlog==-1)
        {
            perror("log file create");
        }
    }

    sprintf(logname,"starting mine id:%d  process starting time:%d sec %d nano sec\n\n",(int)getpid(),(int)tpstart.tv_sec,(int)tpstart.tv_usec);

    if(write(fdlog,logname,strlen(logname))==-1)
        perror("writing log process start time");

    for(i=0;i<size;++i)
    {
        sprintf(logname,"   starting miner id:%d  process starting time:%d sec %d nano sec\n",(int)minerList[i].id,(int)minerList[i].tpstart.tv_sec,(int)minerList[i].tpstart.tv_usec);
        if(write(fdlog,logname,strlen(logname))==-1)
            perror("writing log process start time");
        if(minerList[i].status==1)
        {
            sprintf(logname,"   miner still working\n");
            if(write(fdlog,logname,strlen(logname))==-1)
                perror("writing log process ending time time");
        }
        else if(minerList[i].status==2)
        {
            sprintf(logname,"   miner still waiting entity\n");
            if(write(fdlog,logname,strlen(logname))==-1)
                perror("writing log process ending time time");
        }
        else if(minerList[i].status==3)
        {
            sprintf(logname,"   ending miner id:%d  process ending time:%d sec %d nano sec\n       miner earn:%d golds\n\n",(int)minerList[i].id,(int)minerList[i].tpend.tv_sec,(int)minerList[i].tpend.tv_usec,minerList[i].gold);
            if(write(fdlog,logname,strlen(logname))==-1)
                perror("writing log process start time");
        }
        else
        {
            fprintf(stderr,"miner status error!!\n");
        }
    }

    if(mineStatus==1)
    {
        sprintf(logname,"\nmine still working\n");

        if(write(fdlog,logname,strlen(logname))==-1)
            perror("writing log process start time");
    }
    else if(mineStatus==0)
    {
        timedif = MILLION*(tpend.tv_sec - tpstart.tv_sec) +tpend.tv_usec - tpstart.tv_usec;
        sprintf(logname,"\nctrl+c catched\nending mine id:%d  process ending time:%d sec %d nano sec\n\nprogram took %ld microseconds\n",(int)getpid(),(int)tpstart.tv_sec,(int)tpstart.tv_usec,timedif);

        if(write(fdlog,logname,strlen(logname))==-1)
            perror("writing log process start time");
    }
    else
    {
        fprintf(stderr,"mine status error!!\n");
    }

    sprintf(logname,"\n##############################WORDS############################################\n\n");
    if(write(fdlog,logname,strlen(logname))==-1)
        perror("writing log draw line");
    for(i=0;i<mineSize;++i)
    {
        sprintf(logname,"%s %d times \n",wordList[i].word,wordList[i].numberofword);
        if(write(fdlog,logname,strlen(logname))==-1)
            perror("writing log words\n");
        tw+=wordList[i].numberofword;
    }

    sprintf(logname,"\n%d different words found\n%d total words found\n",mineSize,tw);
    if(write(fdlog,logname,strlen(logname))==-1)
        perror("writing log total words\n");

    sprintf(logname,"\n##############################WORDS############################################\n\n");
    if(write(fdlog,logname,strlen(logname))==-1)
        perror("writing log draw line");

    sprintf(logname,"number of served miners: %d\nnumber of pending miners: %d\nnumber of current working miners: %d\ntotal given coins: %d\n",servedMiners,pendingMiners,workingMiners,allCoin);
    if(write(fdlog,logname,strlen(logname))==-1)
        perror("writing log numeric values\n");
    close(fdlog);
}