// Maylee Gagnon 
// 10.10.2019
// Project4

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>

#define MAXBUFF 1024
#define MAXTHREADS 15

struct statVals {
	int numBadFiles;
	int numDir;
	int numRegFiles;
	int numSpecFiles;
	long long numBytesReg;
	int numAllText;
	long long numBytesText;
};

struct statVals *statStruct; 
pthread_mutex_t lock;
bool isThread; 

/*Checks to see if a regular file contains all text  	
* @param fileName The name of the file being checked 
* returns true if file contains all text
*	  false otherwise 
*/
bool isAllText(char* fileName){  
	char buf[MAXBUFF];
	int fdIn, cnt, i;
	
	if((fdIn = open(fileName, O_RDONLY)) == -1){ 
		fprintf(stderr, "Error: file could not be open\n");
		return false;
	}
	while((cnt = read(fdIn,buf, MAXBUFF)) > 0){	 
		for(i = 0; i < cnt; i++) {					 
			if(!isprint(buf[i]) && !isspace(buf[i])){
				//fprintf(stderr, "Non-printable character\n");
				return false; 
			}
		}
	}
	return true; // end of file reached without problems so file contains all text 
	close(fdIn);
}

/* Checks for the information (ie. file type, size) and updates statVals struct 
 * @param inputLine Name of text file  
*/
void* checkStats(void* name){ 
	struct stat statBuff;
	char* inputLine = (char*)name; 

	if(stat(inputLine, &statBuff) == -1) {
		pthread_mutex_lock(&lock);
			statStruct->numBadFiles++; 
		pthread_mutex_unlock(&lock);
		return NULL;
	} 

	if(S_ISDIR(statBuff.st_mode)) { 
		pthread_mutex_lock(&lock);	
			statStruct->numDir++;
		pthread_mutex_unlock(&lock);
		return NULL;
	}
	if (S_ISREG(statBuff.st_mode)) {
		pthread_mutex_lock(&lock);
			statStruct->numRegFiles++;
		pthread_mutex_unlock(&lock);

		long long size = statBuff.st_size;
		pthread_mutex_lock(&lock);
			statStruct->numBytesReg += size; 
		pthread_mutex_unlock(&lock);
		if(isAllText(inputLine)) { 
			pthread_mutex_lock(&lock);
				statStruct->numAllText++;
				statStruct->numBytesText += size;
			pthread_mutex_unlock(&lock);
		} 	
	}
	if(!S_ISDIR(statBuff.st_mode) && !S_ISREG(statBuff.st_mode)){
		pthread_mutex_lock(&lock);
			statStruct->numSpecFiles++;
		pthread_mutex_unlock(&lock);
	}
	if(isThread) {
		free(name);
	}
}

void printStats(){
	fprintf(stderr, "Bad Files: %d\n", statStruct->numBadFiles);
	fprintf(stderr, "Directories: %d\n", statStruct->numDir);
	fprintf(stderr, "Regular Files: %d\n", statStruct->numRegFiles);
	fprintf(stderr, "Special Files: %d\n", statStruct->numSpecFiles);
	fprintf(stderr, "Regular Files Bytes: %lld\n", statStruct->numBytesReg);
	fprintf(stderr, "Text Files: %d\n", statStruct->numAllText);
	fprintf(stderr, "Text File Bytes: %lld\n", statStruct->numBytesText);
}

int main(int argc, char* argv[]){
	struct timeval startWall, endWall, endSystem, endUser; 
	struct rusage  usageEnd; 
	gettimeofday(&startWall, NULL);

	int x; 
	isThread = false;	
	int maxThreads = 0; 
	int threadCt = 1;

	int posNewThread = 0;
	int posOldestThread = 0; 

	char* inputLine;
	char line[MAXBUFF]; 	
	pthread_t ** threadIds= (pthread_t **)malloc(sizeof(pthread_t *)*MAXTHREADS); 
	
	statStruct = (struct statVals *)malloc(sizeof(struct statVals));
		statStruct->numBadFiles = 0;
		statStruct->numDir = 0;
		statStruct->numRegFiles = 0;
		statStruct->numSpecFiles = 0;
		statStruct->numBytesReg = 0;
		statStruct->numAllText = 0;
		statStruct->numBytesText = 0;	
	if (pthread_mutex_init(&lock, NULL) != 0) {
	 	fprintf(stderr, "mutex failed to create\n");
	}

	if (argc == 3) {
		if(strcmp(argv[1],"thread") == 0) {
			isThread = true;
		}
		int tempNum = atoi(argv[2]);
		if(tempNum > 0 && tempNum <= MAXTHREADS) {
			maxThreads = tempNum;
		} 
		else {
			fprintf(stderr, "Error: thread count [1,15]\n");
			return 0; 
		}
	}
	
	while(1) {
		inputLine = fgets(line, MAXBUFF, stdin); // input has to be smaller than maxbuff ... 
		
		// Trimming new line char and replacing with terminator
		size_t len = strlen(line) -1;
		if(*line && line[len] == '\n') {
			line[len] = '\0';
		}
	
		if(inputLine == NULL) {
			fprintf(stderr, "End of file reached\n");
			break;
		}
		
		if (line[0] == '\n'){
			fprintf(stderr, "Empty line, re-enter\n");
			continue;
		}

		if(isThread) {
			char* fileNameCopy = (char *)malloc(MAXBUFF);
			strcpy(fileNameCopy,line); 
			
			if (threadCt <= maxThreads) { 
				threadIds[posNewThread] = (pthread_t *)malloc(sizeof(pthread_t));		
				if (pthread_create(threadIds[posNewThread], NULL, checkStats, fileNameCopy) != 0){ 
					perror("pthread_create");
					exit(1);
				}
				threadCt++;
				posNewThread++; 
			}		
			else if (threadCt > maxThreads) {  // max threads in use, need to wait for one
				if (posNewThread >= maxThreads) { // need to loop back to start
					posNewThread = 0; 
				}
				if (posOldestThread >= maxThreads) {
					posOldestThread = 0;
				}
		
				(void)pthread_join(*(threadIds[posOldestThread]), NULL);
				if (pthread_create(threadIds[posOldestThread], NULL, checkStats, fileNameCopy) != 0){ 
					perror("pthread_create"); 
					exit(1);
				}
				posNewThread++;  	
				posOldestThread++; 			 
			}	
		}
		else { // Serial 
			checkStats(line);
		}
	}// end while(1)

	if (isThread){
		for (x = 0; x < threadCt-1; x ++) {
			(void)pthread_join(*(threadIds[x]), NULL);
		}
	}

	gettimeofday(&endWall, NULL);
	getrusage(RUSAGE_SELF, &usageEnd);
	endSystem = usageEnd.ru_stime; 
	endUser = usageEnd.ru_utime; 
	
	fprintf(stderr,"Wall-clock (us): %ld\n",(endWall.tv_sec*1000000+endWall.tv_usec)-(startWall.tv_sec*1000000+startWall.tv_usec));
	fprintf(stderr, "User %ld sec %ld usecs\n", endUser.tv_sec, endUser.tv_usec);
	fprintf(stderr, "System %ld sec %ld usecs\n", endSystem.tv_sec, endSystem.tv_usec);
	
	printStats();

	pthread_mutex_destroy(&lock); 
	for (x = 0; x < threadCt-1; x++){
		free(threadIds[x]);	
	}
	free(threadIds); 
	free(statStruct); 
	return 0;
} 
