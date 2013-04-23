int connectionExists=0;
int *currentSock=NULL;
int sock;
int maxCmd=10; 		/* number of subcommand */
int maxCmdSize=1024;	/* size of each subcommand */

#define MAX_MATCHES 1 //The maximum number of matches allowed in a single string

unsigned char *hash(char *fileName){
	unsigned char *hashVal;
	hashVal = ( unsigned char *)malloc( sizeof(unsigned char)*MD5_DIGEST_LENGTH);
	FILE *inFile = fopen (fileName, "rb");
	MD5_CTX mdContext;
	int bytes;
	unsigned char data[1024];

	if (inFile == NULL) {
		printf ("%s can't be opened.\n", fileName);
		return NULL;
	}

	MD5_Init (&mdContext);
	while ((bytes = fread (data, 1, 1024, inFile)) != 0)
		MD5_Update (&mdContext, data, bytes);
	MD5_Final (hashVal,&mdContext);

	fclose (inFile);
	return hashVal;
}
char *hashChunk(char *chunk,int len){
	
	unsigned char *hashVal;
	char  *temp;
	hashVal = ( unsigned char *)malloc( sizeof(unsigned char)*MD5_DIGEST_LENGTH);
	temp = ( char *)malloc( sizeof( char)*MD5_DIGEST_LENGTH*2);
	int i;
	MD5_CTX mdContext;
	
	MD5_Init (&mdContext);
	MD5_Update (&mdContext, chunk, len);
	MD5_Final (hashVal,&mdContext);
	
	for(i = 0; i < MD5_DIGEST_LENGTH; i++) 
		sprintf(temp+i*2,"%02x", hashVal[i]);

	return temp;
}

char *hashing(char *file){
	char *temp;
	temp = ( char *)malloc( sizeof(char)*MD5_DIGEST_LENGTH*2);

	struct stat sb;
	char *path=(char *)malloc(sizeof(char)*100);
	char longListFormat[]="Hash:\t%s\nFile Name:\t%s\nTimestamp(M)\t%s\n\n";
	int oldBlockSize=0,i;
	char *dataBlock;
	DIR* sharedDir, *sharedDir_;                        // Opens a directory stream
	int listCount=0;
	sharedDir_ = opendir(".");
	sharedDir  = opendir(".");

	while(readdir(sharedDir_)) listCount++;      // Count the number of files
	closedir(sharedDir_);

	struct dirent *b;
	dataBlock=(char *)malloc(sizeof(char)*listCount*100);

	while((b = readdir(sharedDir))) {
		if(file && strcmp(file,b->d_name)) continue;
		
		// Returns the structure beloging to the next file in the list
		char *fileName,*lastModTS;
		unsigned char *hashVal;
		strcpy(path,".");
		strcat(path,"/");
		strcat(path,b->d_name);

		if (stat(path, &sb) == -1){
			perror("stat");
			exit(EXIT_SUCCESS);
		}

		fileName=strdup(b->d_name);
		hashVal=hash(fileName);
		if(hashVal==NULL){
			exit(EXIT_SUCCESS);
		}
		lastModTS=strdup(ctime(&sb.st_mtime));
		lastModTS[strlen(lastModTS)-1]='\0';
		for(i = 0; i < MD5_DIGEST_LENGTH; i++) 
			sprintf(temp+i*2,"%02x", hashVal[i]);

		oldBlockSize+=sprintf(dataBlock+oldBlockSize, longListFormat, temp, fileName, lastModTS);
		free(fileName);
	}
	closedir(sharedDir);
	return dataBlock;
}
void errorPrinter(char *err)
{
	printf("ERROR:%s\n>",err);
	fflush(stdout);
}
int match(char *regex, char *str) {
	/*0 for full match , 1 for no match , 2 for error , 3 for partial match*/
	regex_t exp; //Our compiled expression
	int rv;
	//REG_EXTENDED is so that we can use Extended regular expressions
	rv = regcomp(&exp,regex, REG_EXTENDED);

	if (rv != 0) {
		//	printf("regcomp failed with %d\n", rv);
		return 2;
	}
	
	regmatch_t matches[MAX_MATCHES]; //A list of the matches in the string (a list of 1)
	//Compare the string to the expression
	//regexec() returns 0 on match, otherwise REG_NOMATCH
	
	if (regexec(&exp, str, MAX_MATCHES, matches, 0) == 0) {
		int len=strlen(str);
		if( ( matches[0].rm_eo - matches[0].rm_so ) == len ){
	//		printf("\"%s\" matches characters \n", str);
			return 0;
		} else {
	//		printf("\"%s\" matches characters %d - %d\n", str, matches[0].rm_so, matches[0].rm_eo);
			return 3;
		}
	} else {
	//	printf("\"%s\" does not match\n", str);
		return 1;
	}
	regfree(&exp);
}

time_t to_seconds(const char *date){
        struct tm storage={0,0,0,0,0,0,0,0,0};
        time_t retval=0;

        if(!strptime(date,"%Y-%m-%d_%H:%M:%S",&storage)){
                retval=0;
        }
        else{
                retval=mktime(&storage);
        }
        return retval;
}

long long getFileSize(char *fileName){
	char *path=(char *)malloc(sizeof(char)*100);
	strcpy(path,".");
	strcat(path,"/");
	strcat(path,fileName);
	struct stat sb;
	if (stat(path, &sb) == -1){
		perror("stat");
		exit(EXIT_SUCCESS);
	}
	return (long long)sb.st_size;
}

char *listing(char *startTime,char *endTime,char *regex){
	if(regex[0]=='"' && regex[strlen(regex)-1]=='"'){
		regex[strlen(regex)-1]='\0';
		regex++;
	}
	
	DIR* sharedDir, *sharedDir_;                        // Opens a directory stream
	int listCount=0;
	sharedDir_ = opendir("."); 
	sharedDir  = opendir(".");
	while(readdir(sharedDir_)) listCount++;	     // Count the number of files
	closedir(sharedDir_);                                  

	struct dirent *b;
	struct stat sb;
	char *path=(char *)malloc(sizeof(char)*100);
	char *dataBlock=(char *)malloc(listCount*250); 		  
	char longListFormat[]="File Name:\t%s\nFile Size:\t%lld\nTimestamp(M):\t%s\nFile Type:\t%s\n\n";	
	int oldBlockSize=0;

	while((b = readdir(sharedDir))) { 
               // Returns the structure beloging to the next file in the list
		
		char *fileName, *lastModTS, *fileType;
		long long fileSize;
			
		strcpy(path,".");
		strcat(path,"/");
		strcat(path,b->d_name);

		if (stat(path, &sb) == -1){
			perror("stat");
			exit(EXIT_SUCCESS);
		}
		fileName=strdup(b->d_name);
		fileSize=(long long) sb.st_size;
		lastModTS=strdup(ctime(&sb.st_mtime));
	
		switch (sb.st_mode & S_IFMT) {
			case S_IFBLK:  fileType=strdup("block device"); 	   break;
			case S_IFCHR:  fileType=strdup("character device");        break;
			case S_IFDIR:  fileType=strdup("directory");               break;
			case S_IFIFO:  fileType=strdup("FIFO/pipe");               break;
			case S_IFLNK:  fileType=strdup("symlink");                 break;
			case S_IFREG:  fileType=strdup("regular file");            break;
			case S_IFSOCK: fileType=strdup("socket");                  break;
			default:       fileType=strdup("unknown?");                break;
		}
		lastModTS[strlen(lastModTS)-1]='\0';
		if(startTime && endTime){
			time_t t1 = to_seconds(startTime);
			time_t t2 = to_seconds(endTime);
			time_t t3 = sb.st_mtime;
			if(t3<=t2 && t3>=t1){
				oldBlockSize+=sprintf(dataBlock+oldBlockSize, longListFormat, fileName, fileSize, lastModTS, fileType);
			}
		}
		else {
			int result = match(regex,fileName);
			if(result==0){
				oldBlockSize+=sprintf(dataBlock+oldBlockSize, longListFormat, fileName, fileSize, lastModTS, fileType);
			}
		}
	}
	closedir(sharedDir);                                  
	return dataBlock;
}
int min(int a, int b){
	return a<b?a:b;
}


int splitCommand(char **cmd,char *delim,char *str_tmp){
	// split command according to delimeter
        char *token;
        int count=0;

        token = strtok(str_tmp,delim);
        while(token != NULL){
                cmd[count]=strdup(token);
                cmd[count]=strcat(cmd[count],"\0");
                token = strtok(NULL,delim);
                count++;
        }
        return count;
}
int setNonblocking(int fd){
        int flags;
        if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
                flags = 0;
        return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}   
int setBlocking(int fd){
        int flags;
        if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
                flags = 0;
        return fcntl(fd, F_SETFL, flags & (~O_NONBLOCK));
} 
void handleSendData(int currSockFd, char *buff, int len){
	setBlocking(currSockFd);
	char temp[1024];
	char sendData[1024], recvData[1024];
	int blockNo=0, sentDataSize=0;
	int blockSize=len;
	strncpy(sendData,"DATA",4);
	strncpy(sendData+4,"0000",4);
	sprintf(sendData+8,"%08d",blockNo);
	sprintf(sendData+16,"%012d",blockSize);
	send(currSockFd, sendData, strlen(sendData),0);
	//printf("%s\n",sendData);
	while(sentDataSize<=len){
		recv(currSockFd,recvData,sizeof(recvData),0);
		if(atoi(strndup(recvData+8,8))-blockNo==0){
			if(sentDataSize==len) break;
			blockNo++;
			strncpy(sendData,"DATA",4);
			strncpy(sendData+4,"0000",4);
			sprintf(sendData+8,"%08d",blockNo);
			sprintf(sendData+16,"%s",hashChunk(buff+sentDataSize,min(512, blockSize-sentDataSize)));
			memcpy(sendData+48,buff+sentDataSize,min(512, blockSize-sentDataSize));	
			memcpy(temp,buff+sentDataSize,min(512, blockSize-sentDataSize));	
			send(currSockFd, sendData, min(512, blockSize-sentDataSize)+48 ,0);
//			fflush(stdout);
//			write(fileno(stdout),sendData+48,min(512, blockSize-sentDataSize)+48);
//			fflush(stdout);
			sentDataSize+=min(512, blockSize-sentDataSize);
//			printf("!%s\n",sendData);
//			printf("!%s\n",temp);
		}
		else{
			strncpy(sendData,"DATA",4);
			strncpy(sendData+4,"0000",4);
			sprintf(sendData+8,"%08d",blockNo);
			sprintf(sendData+16,"%s",hashChunk(buff+sentDataSize,min(512, blockSize-sentDataSize)));
			memcpy(sendData+48,buff+sentDataSize,min(512, blockSize-sentDataSize));	
			send(currSockFd, sendData, min(512, blockSize-sentDataSize)+48 ,0);
//			printf("!!%s\n",sendData);
		}		
		memset(recvData,'\0',1024);
		memset(sendData,'\0',1024);
	}	
	setNonblocking(currSockFd);
}

int handleRecvData(int currSockFd, char **retVal){
	setBlocking(currSockFd);
	char recvData[1024], sendData[1024];
	recv(currSockFd,recvData,1024,0);
//	printf("Recv:%s\n",recvData);
	int bigBuffSize=atoi(recvData+16), recvDataSize=0;
	char *bigBuff=(char *)malloc(sizeof(char)*bigBuffSize);
	int blockNo=atoi(strndup(recvData+8,8));
//	printf("BlockNo %d\n", blockNo);
//	printf("BigBuffSize %d\n", bigBuffSize);
	strncpy(sendData,"ACK_",4);
	strncpy(sendData+4,"0000",4);
	sprintf(sendData+8,"%08d",blockNo);
	send(currSockFd, sendData, strlen(sendData),0);
//	printf("%s\n",sendData);
	while(recvDataSize<bigBuffSize){
		int tempSize=recv(currSockFd,recvData,sizeof(recvData),0);
//		printf("RecvSize:%d\n",tempSize);
//		printf("RecvData:%s\n",recvData);
//		printf("No:%d",atoi(strndup(recvData+8,8)));
//		printf("RcvDataMD5:%s\n",hashChunk(recvData+48,tempSize-48));
		if(atoi(strndup(recvData+8,8))-blockNo==1
			&& !strncmp(recvData+16,hashChunk(recvData+48,tempSize-48),32))  {
			
			memcpy(bigBuff+recvDataSize,recvData+48,tempSize-48);
//			fflush(stdout);
//			write(fileno(stdout),recvData+48,tempSize-48);
//			fflush(stdout);
			recvDataSize+=tempSize-48;
			blockNo=atoi(strndup(recvData+8,8));
			strncpy(sendData,"ACK_",4);
			strncpy(sendData+4,"0000",4);
			sprintf(sendData+8,"%08d",blockNo);
			sendData[16]='\0';
			send(currSockFd, sendData, strlen(sendData),0);
//			printf("!%s\n",sendData);
		}
		else{
			strncpy(sendData,"ACK_",4);
			strncpy(sendData+4,"0000",4);
			sprintf(sendData+8,"%08d",blockNo);
			sendData[16]='\0';
			send(currSockFd, sendData, strlen(sendData),0);
//			printf("!!%s\n",sendData);
		}
		memset(recvData,'\0',1024);
		memset(sendData,'\0',1024);
	}
	setNonblocking(currSockFd);
	//printf("%s\n",bigBuff);
	*retVal=bigBuff;
	return bigBuffSize;
}

void handleRequest(int currSock, char *inputStr){
	char **outputStr,*temp;
	int i, result=0;
	//int cmdLength;
	/*temp store replica of inputStr*/
	
	outputStr=(char**)malloc(sizeof(char*)*maxCmd);
	for(i=0;i<maxCmd;i++)
		outputStr[i]=(char*)malloc(sizeof(char)*maxCmdSize);

	temp=strdup(inputStr);
	/*cmdLength=*/splitCommand(outputStr," ",temp);
	/* Append CMD_ header */
	char sendData[1024];
	strncpy(sendData,"CMD_",4);

	if(!strcmp(outputStr[0],"Exit")){
		close(sock);
		if(currentSock){
			send(*currentSock,"q",1, 0); 
			close(*currentSock);
		}
		exit(0);
	}
	if(!strcmp(outputStr[0],"IndexGet")){
		if(!result)	result=!strcmp(outputStr[1],"ShortList");
		if(!result)	result=!strcmp(outputStr[1],"LongList");
		if(!result)	result=!strcmp(outputStr[1],"Regex");
		if(result){
			strcpy(sendData+4,inputStr);
			send(currSock, sendData, strlen(sendData), 0);
			char *retVal;
			handleRecvData(currSock, &retVal);
			printf("%s",retVal);
			fflush(stdout);
			free(retVal);
		}
		else
		errorPrinter("Wrong Command");
	}
	else if(!strcmp(outputStr[0],"FileHash")){
		if(!result)	result=!strcmp(outputStr[1],"Verify");
		if(!result)	result=!strcmp(outputStr[1],"CheckAll");
		if(result){
			strcpy(sendData+4,inputStr);
			send(currSock, sendData, strlen(sendData), 0);
			char *retVal;
			handleRecvData(currSock, &retVal);
			printf("%s",retVal);
			fflush(stdout);
			free(retVal);
		}
		else
		errorPrinter("Wrong Command");
	}
	else if(!strcmp(outputStr[0],"FileDownload")){
		result=1;
		strncpy(sendData,"RRQ_",4);
		strncpy(sendData+4,"0000", 4);
		strncpy(sendData+8,inputStr+strlen(outputStr[0])+1,strlen(inputStr+strlen(outputStr[0])));
		sendData[strlen(inputStr+strlen(outputStr[0])+1)+8]='\0';
		send(currSock, sendData, strlen(sendData)+1, 0);
		setBlocking(currSock);
		char recvData[1024];
		recv(currSock,recvData,1024,0);
		long long fileSize=atoi(recvData+8),recievedSize=0, tempSize;
		setNonblocking(currSock);
		FILE *fp=fopen(inputStr+strlen(outputStr[0])+1,"wb");
		while(recievedSize<fileSize){
			char *retVal;
			tempSize=handleRecvData(currSock, &retVal);
			fwrite(retVal, tempSize, 1, fp);
			recievedSize+=tempSize;
			free(retVal);
		}
		fclose(fp);
	}
	else if(!strcmp(outputStr[0],"FileUpload")){
		result=1;
		strncpy(sendData,"WRQ_",4);
		strncpy(sendData+4,"0000", 4);
		strncpy(sendData+8,inputStr+strlen(outputStr[0])+1,strlen(inputStr+strlen(outputStr[0])));
		sendData[strlen(inputStr+strlen(outputStr[0])+1)+8]='\0';
		send(currSock, sendData, strlen(sendData)+1, 0);
		char recvData[1024];
		setBlocking(currSock);
		recv(currSock,recvData,1024,0);
		setNonblocking(currSock);
		if(!strncmp(recvData+8,"Yes",3)){
			FILE *fp=fopen(inputStr+strlen(outputStr[0])+1,"rb");
			int blockNo=0;
			strncpy(sendData,"DATA",4);
			strncpy(sendData+4,"0000",4);
			sprintf(sendData+8,"%08d",blockNo);
			sprintf(sendData+16,"%012lld",getFileSize(inputStr+strlen(outputStr[0])+1));
			setBlocking(currSock);
			send(currSock, sendData, strlen(sendData)+1, 0);
			setNonblocking(currSock);
			while(1){
				char *fileBuff=(char *)malloc(sizeof(char)*1024*1024);
				int bytesRead=(read(fileno(fp),fileBuff,1024*1024));
				if(bytesRead<=0) break;
				else{
					handleSendData(currSock,fileBuff,bytesRead);
				}
				free(fileBuff);
			}
		}
		else{
			printf("\rSorry! Your request has been rejected\n");
		}
	}
	else{
		errorPrinter("Wrong Command");
	}
}



void cmdParser(int currSock, char *inputStr){
	char **outputStr,*temp;
	int i;
	/*temp store replica of inputStr*/
	char sendData[1024], recvData[1024];	

	if(!strncmp(inputStr,"CMD_",4)){
		outputStr=(char**)malloc(sizeof(char*)*maxCmd);
		for(i=0;i<maxCmd;i++)
			outputStr[i]=(char*)malloc(sizeof(char)*maxCmdSize);
		temp=strdup(inputStr+4);
		splitCommand(outputStr," ",temp);

		if(!strcmp(outputStr[0],"Exit")){
			close(sock);
			if(currentSock){
				send(*currentSock,"q",1, 0); 
				close(*currentSock);
			}
			exit(0);
		}
		if(!strcmp(outputStr[0],"IndexGet")){
			if(!strcmp(outputStr[1],"ShortList")){
				char *result=listing(outputStr[2],outputStr[3],".*");
				handleSendData(currSock,result, strlen(result));
				free(result);
			}
			else if(!strcmp(outputStr[1],"LongList")){
				char *result=listing(NULL,NULL,".*");
				handleSendData(currSock,result, strlen(result));
				free(result);
			}
			else if(!strcmp(outputStr[1],"Regex")){
				char *result=listing(NULL,NULL,inputStr+4+strlen(outputStr[0])+strlen(outputStr[1])+2);
				handleSendData(currSock,result, strlen(result));
				free(result);
			}
			else
				errorPrinter("Wrong Command");
		}
		else if(!strcmp(outputStr[0],"FileHash")){
			if(!strcmp(outputStr[1],"Verify")){
				char *result=hashing(inputStr+4+strlen(outputStr[0])+strlen(outputStr[1])+2);
				handleSendData(currSock,result, strlen(result));
				free(result);
			}
			else if(!strcmp(outputStr[1],"CheckAll")){
				char *result=hashing(NULL);
				handleSendData(currSock,result, strlen(result));
				free(result);
			}
			else
				errorPrinter("Wrong Command");
		}
	}
	else if(!strncmp(inputStr,"RRQ_",4)){
		FILE *fp=fopen(inputStr+8,"rb");
		int blockNo=0;
		strncpy(sendData,"DATA",4);
		strncpy(sendData+4,"0000",4);
		sprintf(sendData+8,"%08d",blockNo);
		sprintf(sendData+16,"%012lld",getFileSize(inputStr+8));
		setBlocking(currSock);
		send(currSock, sendData, strlen(sendData)+1, 0);
		setNonblocking(currSock);
		
		while(1){
			char *fileBuff=(char *)malloc(sizeof(char)*1024*1024);
			int bytesRead=(read(fileno(fp),fileBuff,1024*1024));
			if(bytesRead<=0) break;
			else{
				handleSendData(currSock,fileBuff,bytesRead);
			}
			free(fileBuff);
		}
	}
	else if(!strncmp(inputStr,"WRQ_",4)){
		printf("Client wants to share the file \"%s\" with you. Grant Permission? (Y/N)",inputStr+8);
		char reply[2];
		while(1){
			gets(reply);
			if(reply[0]=='Y'||reply[0]=='N')
				break;
			else
				printf("\rImproper response. Try again (Y/N)\n");
		}	
		if(reply[0]=='Y'){
			strncpy(sendData,"ACK_",4);
			strncpy(sendData+4,"0000", 4);
			strncpy(sendData+8,"Yes",3);
			sendData[11]='\0';
			send(currSock, sendData, strlen(sendData)+1, 0);
			setBlocking(currSock);
			recv(currSock,recvData,1024,0);
			long long fileSize=atoi(recvData+8),recievedSize=0, tempSize;
			setNonblocking(currSock);
			FILE *fp=fopen(inputStr+8,"wb");
			while(recievedSize<fileSize){
				char *retVal;
				tempSize=handleRecvData(currSock, &retVal);
				fwrite(retVal, tempSize, 1, fp);
				recievedSize+=tempSize;
				free(retVal);
			}
			fclose(fp);
		}
		else {
			strncpy(sendData,"ACK_",4);
			strncpy(sendData+4,"0000", 4);
			strncpy(sendData+8,"No",2);
			sendData[10]='\0';
			send(currSock, sendData, strlen(sendData)+1, 0);
		}
	}
	else{
		errorPrinter("Wrong Command");
		return;
	}

	return;
}
