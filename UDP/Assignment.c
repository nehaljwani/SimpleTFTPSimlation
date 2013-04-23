/* Authors: (i)	Nehal J Wani 201125005	nehal.wani@students.iiit.ac.in
	   (ii)	Mayank Gupta 201101004	mayank.gupta@students.iiit.ac.in
  TFTP Application Layer Service
*/
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <dirent.h>
#include <regex.h>
#include <openssl/md5.h>

#ifndef __USE_XOPEN
#define __USE_XOPEN
#endif
#include <time.h>
#include "cmdparser.h" 

struct history{
	char command[1024];
	struct history *next;	
};

struct history* lastCommand;

struct history* pushCommand(struct history *cmdHistory, char *currCommand){
	struct history *cmdPtr=cmdHistory;
	while(cmdPtr->next!=NULL)
		cmdPtr=cmdPtr->next;
	cmdPtr->next=(struct history *)malloc(sizeof(struct history));
	strncpy(cmdPtr->next->command,currCommand,strlen(currCommand));
	cmdPtr->next->next=NULL;
	return cmdPtr->next;
}
int waitForStdin(int seconds){
	fd_set set;
	struct timeval timeout = { 0, seconds };
	FD_ZERO(&set);
	FD_SET(0, &set);
	return select(1, &set, NULL, NULL, &timeout) == 1;
}
void signalCallbackHandler(int signum) {
	printf("\rCaught signal %d\n",signum);
	close(sock);
	if(currentSock){
		send(*currentSock,"q",1, 0);
		close(*currentSock);
	}
	exit(signum);
}
int isValidIpAddress(char *ipAddress){
	struct sockaddr_in sa;
	int result = inet_pton(AF_INET, ipAddress, &(sa.sin_addr));
	if (!result) result = !strcmp(ipAddress,"localhost");
	return result;
}
int main(int argc, char **argv){
	int bytes_recieved ,  listen_port, dest_port;  
	char send_data [1024] , recv_data[1024], command_buff[1024];       
	
	struct sockaddr_in server_addr,client_addr;    

	signal(SIGINT, signalCallbackHandler);

	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("Socket");
		exit(1);
	}
	currentSock=&sock;
	if(argc!=2){
		printf("Usage: ./<program> <listening_port>\n");
		exit(-1);
	}
	
	listen_port=atoi(argv[1]);
	server_addr.sin_family = AF_INET;         
	server_addr.sin_port = htons(listen_port);     
	server_addr.sin_addr.s_addr = INADDR_ANY; 
	bzero(&(server_addr.sin_zero),8); 

	if (bind(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr))== -1) {
		perror("Unable to bind");
		exit(1);
	}

	printf("FTPTServer waiting for client on port %d\n>", listen_port);
	fflush(stdout);
	
	setNonblocking(sock);
	struct history *commandHistory=(struct history *)malloc(sizeof(struct history));
	commandHistory->next=NULL;
	int addr_len = sizeof(struct sockaddr);

	while(1){  
		if((bytes_recieved = recvfrom(sock,recv_data,1024,0,(struct sockaddr *)&client_addr, (socklen_t *)&addr_len))!=-1){
			recv_data[bytes_recieved] = '\0';
			printf("\r(%s , %d) said : ",inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));
			printf("%s\n", recv_data);
			printf(">");
			fflush(stdout);
			memset(recv_data,'\0',1024);
			while(1){
				if(waitForStdin(1)){
					gets(send_data);
					if ((strcmp(send_data , "q") == 0) || strcmp(send_data , "Q") == 0)
						break;
					else{
						printf(">");
						fflush(stdout);
						//		handleRequest(sock2, send_data);
						handleRequest(sock, send_data, (struct sockaddr *)&client_addr);
						printf("\n>");
						fflush(stdout);
						memset(send_data,'\0',1024);
					}
				}	
				else if((bytes_recieved = recvfrom(sock,recv_data,1024,0,
								(struct sockaddr *)&client_addr, (socklen_t *)&addr_len))!=-1){
					printf("\rCommand recieved:%s\n",recv_data);
					fflush(stdout);
					cmdParser(sock,recv_data,(struct sockaddr *)&client_addr);
					printf("\n>");
					fflush(stdout);
					memset(recv_data,'\0',1024);
				}
			}

		}
		else{
			if(waitForStdin(1)){
				gets(command_buff);
				lastCommand=pushCommand(commandHistory,command_buff);
				int sock2, i, noCmd;
				char **outputStr;
				outputStr=(char**)malloc(sizeof(char*)*maxCmd);
				for(i=0;i<maxCmd;i++)
					outputStr[i]=(char*)malloc(sizeof(char)*maxCmdSize);
				noCmd=splitCommand((char **)outputStr," ",command_buff);
				if(noCmd==3 && !strncmp(command_buff,"CONNECT",7)){
					struct sockaddr_in server_addr2;
					struct hostent *host;
					if(!isValidIpAddress(outputStr[1])){
						printf("\rNot a valid address!\n");
						printf(">");
						fflush(stdout);
						continue;
					}
					printf(">");
					fflush(stdout);

					host = gethostbyname(outputStr[1]);

					if ((sock2 = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
						perror("socket");
						exit(1);
					}

					dest_port=atoi(outputStr[2]);
					server_addr2.sin_family = AF_INET;
					server_addr2.sin_port = htons(dest_port);
					server_addr2.sin_addr = *((struct in_addr *)host->h_addr);
					bzero(&(server_addr2.sin_zero),8);
					setNonblocking(sock2);
					fflush(stdout);
					sendto(sock2, command_buff, strlen(command_buff), 0,
							(struct sockaddr *)&server_addr2, sizeof(struct sockaddr));
					while(1){
						if(waitForStdin(1)){
							gets(send_data);
							if ((strcmp(send_data , "q") == 0) || strcmp(send_data , "Q") == 0)
								break;
							else{
								handleRequest(sock2, send_data, (struct sockaddr *)&server_addr2);
								printf("\n>");
								fflush(stdout);
								//				handleRequest(sock2, send_data);
								memset(send_data,'\0',1024);
							}
						}
						else if((bytes_recieved = recvfrom(sock2,recv_data,1024,0,
										(struct sockaddr *)&server_addr2, (socklen_t *)&addr_len))!=-1){
								printf("\rCommand recieved:%s\n",recv_data);
								cmdParser(sock2,recv_data,(struct sockaddr *)&server_addr2);
								printf(">");
								fflush(stdout);
								memset(recv_data,'\0',1024);
						}

					}
				}
				else{
					//					cmdParser(sock2, command_buff);
				}
			  }
		}
	}       
	close(sock);
	return 0;
} 
