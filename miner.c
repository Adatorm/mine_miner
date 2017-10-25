/*######################################################*/
/*                                                      */
/*Guner Acet 121044049                                  */
/*                                                      */
/*System Programming final      miner                   */
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
#include <sys/msg.h>
#include "mylib.h"

int getEntities(entity_t *entityArr,int *size,sem_t *sema);/*get entities from mine and return allocated list*/
void *searchFile(void *data);/*thread function return wordlist*/
void saveWord(char *word,word_t **array,int *size,int *capacity);/*save word in given array*/
void saveWord2(char *word,word_t **array,int *size,int *capacity);/*save word in given array*/
int wordSep(char c);/*separate words*/
int isWord(char word[WORD_MAX],int size);/*check given word*/
void joinList(word_t **mainArr,word_t *subArr,int *mainSize,int *mainCapacity);/*collect threads words*/
void updateLog(int wordSize,int fileListSize,int maxFileSize);/*update log file*/


static int idd;/*shared memory id*/
static int idd2;/*shared memory id2 */
static int fdlog;/*log file*/
static fileList_t *entityArrLocal;/*local entity*/
static word_t *minerWordArr;/*miner total word*/
static int totalCoin;/*total coin*/
static struct timeval tpstart={-1,-1};/*start time*/
static struct timeval tpend={-1,-1};/*end time*/
static int totalNumberOfFile=0;/*total number of file*/
static int totalNumberOfReadedFile=0;/*total number of readed file*/
static char logFileName[WORD_MAX];/*log file name*/
static int deletedFile=0;/*deleted files*/
static int findingDirectory=0;/*number of finding directories*/
static int pendingEntity=0;/*number of needed entity*/
static int minerStatus=0;/*miner status,0 not working,1-working,2-pending*/
static int iMinerSize=0;/*miner word array size*/
static sem_t *mysem,*mysem2;/*semaphore for shared memory*/

void SEGFAULThandler(int sig)
{
    fprintf(stderr,"SIGINT received\n");
    if (gettimeofday(&tpend, NULL)) /*get starting time*/
    {
        fprintf(stderr, "Failed to get end time\n");
    }
    minerStatus=3;
    updateLog(iMinerSize,totalNumberOfReadedFile,totalNumberOfFile);/*update log file*/
    free(entityArrLocal);
    free(minerWordArr);
    sem_close(mysem2);
    sem_close(mysem);

    exit(0);
}

void wakeyWakey(int sig)
{
    fprintf(stderr,"miners ready\n");
}

int main(int argc,char **argv){
    int i;/*increment*/
    key_t mykey,mykey2;
    int n;/*given n*/
    int shmid,shmid2;/*shared memory id*/
    int *sharedSize;/*shared memory size*/
    entity_t *entityArrShared;/*shared array*/
    int iMinerCapacity=0;/*miner capacity*/
    word_t *result;/*thread return list*/
    entity_t *entityTemp;/*after fetching*/
    int iNewSize;/*new shared memory size*/
    int fd;/*fifo*/
    int fd2;
    int fd3;/*fifo for paying*/
    fifodat_t data;/*data for fifo*/
    DIR *dirp;/*check entity with opendir*/
    int error;/*error handling*/
    int iInitializer=0;/*for loop init*/
    int iRemainingEntity;/*number of needed entity*/
    int isExist;/*using paying gold*/
    int cost;/*value word*/
    data.processid=-1;
    data.entity=0;
    data.operation=0;
    minerStatus=1;

    strcpy(data.path,"empty");

    signal(SIGSEGV, SEGFAULThandler);     /*install the handler    */
    signal(SIGINT, SEGFAULThandler);     /*install the handler    */
    signal(SIGUSR1,wakeyWakey);          /*install the handler    */
    signal(SIGABRT,SEGFAULThandler);          /*install the handler    */

    if (gettimeofday(&tpstart, NULL)) /*get starting time*/
    {
        fprintf(stderr, "Failed to get start time\n");
    }
    sprintf(logFileName,"%d",getpid());
    strcat(logFileName,"-clientlog.txt");

    updateLog(iMinerSize,totalNumberOfReadedFile,totalNumberOfFile);/*update log file*/

    if(argc !=2)
    {
        perror("illegal arguman,more or less parameters\nUsage:miner -<n>\n");
        return 1;
    }
    if(sscanf(&argv[1][1],"%d",&n)!=1)
    {
        perror("illegal n value\nUsage:miner -<n>\n");
        return 1;
    }
    if(n < 1)
    {
        perror("negative n value,process terminated\nUsage:miner -<n>\n");
        return 1;
    }


    if ((mykey = ftok(KEY_PATH, 1)) == (key_t)-1)
    {
        perror("Failed to derive key");
        return 1;
    }

    if ((mykey2 = ftok(KEY_PATH, 2)) == (key_t)-1)
    {
        perror("Failed to derive key2");
        return 1;
    }

    updateLog(iMinerSize,totalNumberOfReadedFile,totalNumberOfFile);/*update log file*/

    mysem=sem_open(MYSEMAPHORE,O_CREAT,PERMS,1);


    mysem2=sem_open(MYSEMAPHORE2,O_CREAT,PERMS,1);

    /*start of critical section*/
    /*--------------------------------------------------------*/
    if(sem_wait(mysem)==-1)
    {
        perror("semaphore locking");
    }

    if ((shmid = shmget(mykey,sizeof(int), PERMS )) == -1) {
        perror("Failed to open shared memory segment");
    }

    idd=shmid;
    sharedSize=(int*)shmat(shmid,NULL,0);

    if(sharedSize==NULL)
    {
        perror("sharedSize is null");
    }
    
    fprintf(stderr,"mine have %d entity\n",*sharedSize);
    if(*sharedSize!=0)
    {
        if ((shmid2 = shmget(mykey2,sizeof(entity_t)*(*sharedSize), PERMS)) == -1)
        {
            perror("Failed to open shared memory2 segment");
        }
        idd2=shmid2;
        entityArrShared=(entity_t*)shmat(shmid2,NULL,0);

        if(entityArrShared==NULL)
        {
            perror("entityArrShared is null");
        }
    }   
    
    if(*sharedSize==0)
    {
        shmdt((void *) entityArrShared);

        if(sem_post(mysem)==-1)
        {
            perror("semaphore unlocking\n");
        }

        /*------------------------------------------------------------*/
        /*end of critical section*/


        /*********************************************/
        /*strt*/
        if(sem_wait(mysem2)==-1)
        {
            perror("semaphore locking");
        }
        
        data.operation=2;
        data.processid=getpid();
        data.entity=n;

        fd=open(MYFIFO,O_WRONLY);
        if(fd==-1)
        {
            perror("open fifo failed in miner");
        }
        
        if(write(fd,&data,sizeof(fifodat_t))==-1)
        {
            perror("miner-working inform");
        }

        close(fd);

        if(sem_post(mysem2)==-1)
        {
            perror("semaphore unlocking\n");
        }
        /***************************************************/
        /*end*/

        pendingEntity=n;
        minerStatus=2;
        updateLog(iMinerSize,totalNumberOfReadedFile,totalNumberOfFile);/*update log file*/
        fprintf(stderr,"pending...\n");

        pause();

        /*start of critical section*/
        /*--------------------------------------------------------*/
        if(sem_wait(mysem)==-1)
        {
            perror("semaphore locking");
        }
        fprintf(stderr,"shared size is zero\n");
        minerStatus=1;
    }
    else if(*sharedSize < n)
    {
        iRemainingEntity=n-*sharedSize;
        entityArrLocal=(fileList_t*)malloc(sizeof(fileList_t)*n);
        iInitializer=*sharedSize;
        for(i=0;i<iInitializer;++i)
        {
            strcpy(entityArrLocal[i].path,entityArrShared[i].entity);
        }
        shmdt((void *) entityArrShared);
        shmctl(shmid2, IPC_RMID, NULL);
        *sharedSize=0;

        shmdt((void *) entityArrShared);

        if(sem_post(mysem)==-1)
        {
            perror("semaphore unlocking\n");
        }
        /*------------------------------------------------------------*/
        /*end of critical section*/


        /*********************************************/
        /*strt*/
        if(sem_wait(mysem2)==-1)
        {
            perror("semaphore locking");
        }
        
        data.operation=2;
        data.processid=getpid();
        data.entity=iRemainingEntity;

        fd=open(MYFIFO,O_WRONLY);
        if(fd==-1)
        {
            perror("open fifo failed in miner");
        }
        
        if(write(fd,&data,sizeof(fifodat_t))==-1)
        {
            perror("miner-working inform");
        }

        close(fd);

        if(sem_post(mysem2)==-1)
        {
            perror("semaphore unlocking\n");
        }
        /***************************************************/
        /*end*/

        pendingEntity=iRemainingEntity;
        minerStatus=2;
        updateLog(iMinerSize,totalNumberOfReadedFile,totalNumberOfFile);/*update log file*/
        fprintf(stderr,"pending...\n");
        pause();

        /*start of critical section*/
        /*--------------------------------------------------------*/
        if(sem_wait(mysem)==-1)
        {
            perror("semaphore locking");
        }
        minerStatus=1;

        fprintf(stderr, "yer yok bacım\n" );

    }

    if ((shmid2 = shmget(mykey2,sizeof(entity_t)*(*sharedSize), PERMS)) == -1)
    {
        perror("Failed to open shared memory2 segment");
    }
    idd2=shmid2;
    entityArrShared=(entity_t*)shmat(shmid2,NULL,0);

    if(entityArrShared==NULL)
    {
        perror("entityArrShared is null");
    }

    if(iInitializer==0)
    {
        entityArrLocal=(fileList_t*)malloc(sizeof(fileList_t)*n);
        iRemainingEntity=n;
    }
    iNewSize=*sharedSize-n;
    for(i=0;i<iRemainingEntity;++i)
    {
        strcpy(entityArrLocal[iInitializer+i].path,entityArrShared[i].entity);
    }

    entityTemp=(entity_t*)malloc(sizeof(entity_t)*iNewSize);

    for(i=0;i<iNewSize;++i)
    {
        strcpy(entityTemp[i].entity,entityArrShared[i+iRemainingEntity].entity);
    }

    if(*sharedSize!=0)
    {
        shmdt((void *) entityArrShared);
        shmctl(shmid2, IPC_RMID, NULL);
    }

    if(iNewSize>0)
    {
        if ((shmid2 = shmget(mykey2,sizeof(entity_t)*iNewSize, PERMS | IPC_CREAT)) == -1) {
        perror("Failed to create shared memory2 segment");
        }
        idd2=shmid2;
        
        entityArrShared=(entity_t*)shmat(shmid2,NULL,0);
        if(entityArrShared==NULL)
        {
            perror("entityArrShared is null");
        }
    }
    
    *sharedSize=iNewSize;
    for(i=0;i<iNewSize;++i)
    {
        strcpy(entityArrShared[i].entity,entityTemp[i].entity);
    }
    free(entityTemp);
    shmdt((void *) sharedSize);
    if(iNewSize>0)
        shmdt((void *) entityArrShared);

    if(sem_post(mysem)==-1)
    {
        perror("semaphore unlocking\n");
    }

    /*------------------------------------------------------------*/
    /*end of critical section*/
    updateLog(iMinerSize,totalNumberOfReadedFile,totalNumberOfFile);/*update log file*/
    /*********************************************/
    /*strt*/
    if(sem_wait(mysem2)==-1)
    {
        perror("semaphore locking");
    }
    
    data.operation=4;

    data.processid=getpid();

    fd=open(MYFIFO,O_WRONLY);
    if(fd==-1)
    {
        perror("open fifo failed in miner");
    }

    if(write(fd,&data,sizeof(fifodat_t))==-1)
    {
        perror("miner-working inform");
    }
    close(fd);

    if(sem_post(mysem2)==-1)
    {
        perror("semaphore unlocking\n");
    }
    /***************************************************/
    /*end*/

    for(i=0;i<n;++i)/*open entity*/
    {

        updateLog(iMinerSize,totalNumberOfReadedFile,totalNumberOfFile);/*update log file*/
        dirp=opendir(entityArrLocal[i].path);
        if(dirp==NULL)
        {
            entityArrLocal[i].tpstart.tv_sec=-1;
            entityArrLocal[i].tpend.tv_sec=-1;
            if((error=pthread_create(&entityArrLocal[i].id,NULL,searchFile,&entityArrLocal[i].path)))
            {
                fprintf(stderr,"Failed to create thread: %s\n",strerror(error));
            }
            if (gettimeofday(&entityArrLocal[i].tpstart, NULL)) /*get starting time*/
            {
                fprintf(stderr, "Failed to get start time\n");
            }
            totalNumberOfFile++;
        }
        else/*entity is dir report*/
        {
            closedir(dirp);
            entityArrLocal[i].id=-1000;
            data.operation=3;
            strcpy(data.path,entityArrLocal[i].path);

            /*********************************************/
            /*strt*/
            if(sem_wait(mysem2)==-1)
            {
                perror("semaphore locking");
            }
            
            fd=open(MYFIFO,O_WRONLY);

            if(fd==-1)
            {
                perror("open fifo failed in miner");
            }

            if(write(fd,&data,sizeof(fifodat_t))==-1)
            {
                perror("miner-working inform");
            }
            close(fd);
            findingDirectory++;
            /*report*/
            if(sem_post(mysem2)==-1)
            {
                perror("semaphore unlocking\n");
            }
            /***************************************************/
            /*end*/
        }
    }
    for(i=0;i<n;++i)/*wait thread*/
    {

        updateLog(iMinerSize,totalNumberOfReadedFile,totalNumberOfFile);/*update log file*/
        if(entityArrLocal[i].id!=-1000)
        {
            pthread_join(entityArrLocal[i].id,(void**)&result);
            if (gettimeofday(&entityArrLocal[i].tpend, NULL)) /*get ending time*/
            {
                fprintf(stderr, "Failed to get end time\n");
            }
            if(result==NULL)
            {
                fprintf(stderr,"%s file was deleted!\n",entityArrLocal[i].path);
                deletedFile++;
            }
            else if(result[0].numberofword==0)
            {
                totalNumberOfReadedFile++;
                free(result);
            }
            else
            {   
                joinList(&minerWordArr,result,&iMinerSize,&iMinerCapacity);
                free(result);
                totalNumberOfReadedFile++;
            }
        }
    }
    /*start of critical section*/
    /*--------------------------------------------------------*/
    if(sem_wait(mysem2)==-1)
    {
        perror("semaphore locking");
    }
    
    data.operation=1;

    data.processid=getpid();

    fd=open(MYFIFO, O_WRONLY );
    if(fd==-1)
    {
        perror("open fifo failed in miner");
    }

    if(write(fd,&data,sizeof(fifodat_t))==-1)
    {
        perror("miner-selling word");
    }

    fd3=open(MYFIFO3, O_RDONLY);
    if(fd3==-1)
    {
        perror("open fifo3 failed in miner");
    }
    fd2=open(MYFIFO2, O_WRONLY );
    if(fd2==-1)
    {
        perror("open fifo2 failed in miner");
        fprintf(stderr, "%d\n",errno );
    }
    
    
    for(i=1;i<iMinerSize+1;++i)
    {
        if(write(fd2,&minerWordArr[i],sizeof(word_t))==-1)
        {
            perror("miner-seeling word");
        }
        if(read(fd3,&isExist,sizeof(int))==-1)
        {
            perror("miner-reading rare");
        }
        if(isExist==1)
            cost=minerWordArr[i].numberofword;
        else if(isExist==0)
            cost=10+minerWordArr[i].numberofword-1;
        else
        {
            perror("rare value not legal");
        }
        totalCoin+=cost;
        minerWordArr[i].rare=cost;
    }
    close(fd);
    close(fd2);
    close(fd3);

    if(sem_post(mysem2)==-1)
    {
        perror("semaphore unlocking\n");
    }

    /*------------------------------------------------------------*/
    /*end of critical section*/

    if (gettimeofday(&tpend, NULL)) /*get starting time*/
    {
        fprintf(stderr, "Failed to get end time\n");
        return 1;
    }

    updateLog(iMinerSize,totalNumberOfReadedFile,totalNumberOfFile);/*update log file*/

    free(entityArrLocal);

    free(minerWordArr);

    sem_close(mysem2);

    sem_close(mysem);

    fprintf(stderr,"the program is over\n");

    return 0;
}

/**searchFile function open file and search it
 *@param path path for opening file
 *@return if successful return wordlist array pointer,if file is not exist,return null
 */
void *searchFile(void *data)
{
    int fd;
    int iIndex;
    int iStatus=1;
    word_t *wordArr;
    int size=0;
    int capacity=0;
    char word[WORD_MAX];
    char c;
    fd=open((char*)data,O_RDONLY);
    if(fd==-1)
    {
        fprintf(stderr, "nullllllllllllllllllllllll\n");
        return NULL;
    }
    while(iStatus!=0 && iStatus!=-1)
        {
            iIndex=0;
            do
            {
                iStatus=read(fd,&word[iIndex],sizeof(char));
                c=word[iIndex];
                if(iStatus==0 || wordSep(c))
                {
                    break;
                }
                else if(iStatus==-1)
                {
                    perror("read -1 dondu!!");
                }
                ++iIndex;
            }while(iIndex<WORD_MAX);
            word[iIndex]='\0';
            if(isWord(word,iIndex)==1)
            {
                saveWord(word,&wordArr,&size,&capacity);
            }
        }
        close(fd);
        if(size==0)
        {
            wordArr=(word_t*)malloc(sizeof(word_t));
        }
        wordArr[0].numberofword=size;
        return wordArr;
}


/**
 *saveWord function takes a word if the word exist in array,increase number of the word,
 *otherwise reallocate array and add the word to array
 *@param word char pointer for search or add a word
 */
void saveWord(char *word,word_t **array,int *size,int *capacity)
{
    if(*size==0)
    {
        *capacity=10;
        *array=(word_t*)malloc(sizeof(word_t)*(*capacity));
    }

    if(*size==*capacity-2)
    {
        *capacity+=20;
        *array=(word_t*)realloc(*array,sizeof(word_t)*(*capacity));
    }
    strcpy((*array)[*size+1].word,word);
    (*array)[*size+1].numberofword=1;
    (*array)[*size+1].rare=-1;
    *size+=1;

    return;
}

/**
 *saveWord function takes a word if the word exist in array,increase number of the word,
 *otherwise reallocate array and add the word to array
 *@param word char pointer for search or add a word
 */
void saveWord2(char *word,word_t **array,int *size,int *capacity)
{
    int i;
    if(*size==0)
    {
        *capacity=10;
        *array=(word_t*)malloc(sizeof(word_t)*(*capacity));
    }
    for(i=1;i<*size+1;++i)
    {
        if(strcmp((*array)[i].word,word)==0)
        {
            (*array)[i].numberofword+=1;
            return;
        }
    }
    if(*size==*capacity-2)
    {
        *capacity+=20;
        *array=(word_t*)realloc(*array,sizeof(word_t)*(*capacity));
    }
    strcpy((*array)[*size+1].word,word);
    (*array)[*size+1].numberofword=1;
    (*array)[*size+1].rare=-1;
    *size+=1;
    return;
}

/**
 *wordSep function using in wordCounter functions for seperating words
 *@param c take char for cheching
 *@return 1 if c is seperator,otherwise return 0
 */
int wordSep(char c)
{
    if(c==' ' || c==',' || c=='.' || c=='\n' || c==':' || c==';')
        return 1;
    else
        return 0;
}

/**
 *isWord function check given word
 *@param word char array is given word for checking
 *@size sizeof word array
 *@return 1 if given word is pass all rules otherwise return 0
 */
int isWord(char word[WORD_MAX],int size)
{
    int index=0;
    char c;
    if(size<2)
        return 0;
    if(size==2)
    {
        if(isalpha(word[0]))
            return 1;
        else
            return 0;
    }
    else if(size==1)
        return 0;
    do
    {
        c=word[index];
        if(isdigit(c))
        {
            return 0;
        }
        index+=1;
    }while(size>index);
    return 1;
}
/**joinList function upgate mainArr
 *@param mainArr main array pointer
 *@param subArr sub array pointer
 *@param mainSize size of main array
 *@param mainCapacity size of main capacity
 */
void joinList(word_t **mainArr,word_t *subArr,int *mainSize,int *mainCapacity)
{
    int i;
    for(i=1;i<subArr[0].numberofword+1;++i)
    {
        saveWord2(subArr[i].word,mainArr,mainSize,mainCapacity);
    }
}


void updateLog(int wordSize,int fileListSize,int maxFileSize)/*update log file*/
{
    char logname[WORD_MAX];
    int i;
    long timedif;
    fdlog=open(logFileName,WRITE_FLAGS,WRITE_PERMS);
    if(fdlog==-1)
    {
        if(errno==9)
        {
            close(fdlog);
            perror("close log file");
        }
        unlink(logFileName);
        fdlog=open(logFileName,WRITE_FLAGS,WRITE_PERMS);
        if(fdlog==-1)
        {
            perror("log file create");
        }
    }
    sprintf(logname,"starting miner id:%d  process starting time:%d sec %d nano sec\n\n",(int)getpid(),(int)tpstart.tv_sec,(int)tpstart.tv_usec);

    if(write(fdlog,logname,strlen(logname))==-1)
        perror("writing log process start time");

    for(i=0;i<fileListSize;++i)
    {
        if(entityArrLocal[i].id!=-1000)
        {
            sprintf(logname,"  start thread id:%ld  thread starting time:%d sec %d nano sec\n",(long)entityArrLocal[i].id,(int)entityArrLocal[i].tpstart.tv_sec,(int)entityArrLocal[i].tpstart.tv_usec);
            if(write(fdlog,logname,strlen(logname))==-1)
                perror("writing log thread start time");
            if(entityArrLocal[i].tpend.tv_sec==-1)
            {
                sprintf(logname,"  thread still working...\n");
                if(write(fdlog,logname,strlen(logname))==-1)
                    perror("writing log thread working");
            }
            else
            {
                sprintf(logname,"  end thread id:%ld   thread ending time:%d sec %d nano sec\n",(long)entityArrLocal[i].id,(int)entityArrLocal[i].tpend.tv_sec,(int)entityArrLocal[i].tpend.tv_usec);
                if(write(fdlog,logname,strlen(logname))==-1)
                    perror("writing log process end time");
            }
        }
    }
    if(tpend.tv_sec==-1)
    {
        if(minerStatus==2)
            sprintf(logname,"miner still wating entity...\n\n");
        else
            sprintf(logname,"miner still working...\n\n");
        if(write(fdlog,logname,strlen(logname))==-1)
            perror("writing log process status");
    }
    else
    {
        if(minerStatus==3)
            sprintf(logname,"\nctrl+c catched\nend process id:%d  process ending time:%d sec %d nano sec\n",(int)getpid(),(int)tpend.tv_sec,(int)tpend.tv_usec);
        else
            sprintf(logname,"\nend process id:%d  process ending time:%d sec %d nano sec\n",(int)getpid(),(int)tpend.tv_sec,(int)tpend.tv_usec);
            if(write(fdlog,logname,strlen(logname))==-1)
                perror("writing log process end time");
    }

    sprintf(logname,"\n##############################WORDS############################################\n\n");
    if(write(fdlog,logname,strlen(logname))==-1)
        perror("writing log draw line");
    for(i=1;i<wordSize+1;++i)
    {
        sprintf(logname,"%s %d times %d coin\n",minerWordArr[i].word,minerWordArr[i].numberofword,minerWordArr[i].rare);
        if(write(fdlog,logname,strlen(logname))==-1)
            perror("writing log words");
    }


    sprintf(logname,"\n##############################WORDS############################################\n\n");
    if(write(fdlog,logname,strlen(logname))==-1)
        perror("writing log draw line");
    sprintf(logname,"number of files that the miner worked on:%d\nnumber of pending files:%d\ndeleted files:%d\nfinding directory:%d\ntotal earned gold:%d\n",totalNumberOfReadedFile,pendingEntity,deletedFile,findingDirectory,totalCoin);
    if(write(fdlog,logname,strlen(logname))==-1)
        perror("writing log numeratic values");
    if(tpend.tv_sec!=-1)
    {
        timedif = MILLION*(tpend.tv_sec - tpstart.tv_sec) +tpend.tv_usec - tpstart.tv_usec;
        sprintf(logname,"The program took %ld microseconds\n",timedif);
        if(write(fdlog,logname,strlen(logname))==-1)
            perror("writing log draw line");
    }
    close(fdlog);
}