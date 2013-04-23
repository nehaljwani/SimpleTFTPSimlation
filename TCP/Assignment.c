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
	int connected, bytes_recieved ,  listen_port, dest_port;  
	char send_data [1024] , recv_data[1024], command_buff[1024];       
	
	struct sockaddr_in server_addr,client_addr;    
	int sin_size;

	signal(SIGINT, signalCallbackHandler);

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
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

	if (listen(sock, 5) == -1) {
		perror("Listen");
		exit(1);
	}

	printf("FTPTServer waiting for client on port %d\n>", listen_port);
	fflush(stdout);
	
	setNonblocking(sock);
	struct history *commandHistory=(struct history *)malloc(sizeof(struct history));
	commandHistory->next=NULL;

	while(1){  

		sin_size = sizeof(struct sockaddr_in);

		if(!connectionExists && ((connected = accept(sock, (struct sockaddr *)&client_addr,(socklen_t *)&sin_size))!=-1)){
			connectionExists=1;
			currentSock=&connected;
			setNonblocking(connected);
			printf("\rI got a connection from (%s , %d)",inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));
			printf("\n>");
			fflush(stdout);
			while (1){
				if(waitForStdin(1)){
					gets(send_data);
					lastCommand=pushCommand(commandHistory,send_data);
					if (strcmp(send_data , "q") == 0 || strcmp(send_data , "Q") == 0){
						send(connected, send_data,strlen(send_data), 0); 
						memset(send_data,'\0',1024);
						printf("\rYou closed connection with %s:%d\n", 
							inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
						printf(">");
						fflush(stdout);
						close(connected);
						connectionExists=0;
						break;
					}
					else{
						handleRequest(connected, send_data);
						memset(send_data,'\0',1024);
						printf(">");
						fflush(stdout);
					}
				}
				else if((bytes_recieved = recv(connected,recv_data,1024,0))!=-1){
						recv_data[bytes_recieved] = '\0';
						lastCommand=pushCommand(commandHistory,recv_data);
						if (strcmp(recv_data , "q") == 0 || strcmp(recv_data , "Q") == 0){
							printf("\r%s:%d closed connection with you\n",
							 inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));
							close(connected);
							connectionExists=0;
							printf(">");
							fflush(stdout);
							memset(recv_data,'\0',1024);
							break;
						}
						else if(strlen(recv_data)){
							printf("\rCommand recieved = %s" , recv_data);
							fflush(stdout);
							cmdParser(connected, recv_data);
							memset(recv_data,'\0',1024);
							printf("\n>");
							fflush(stdout);
						}
				}
			}
		}
		else{
			if(waitForStdin(1)){
				/* We are now in the connection phase */
				gets(command_buff);
				lastCommand=pushCommand(commandHistory,command_buff);
				int sock2, i, noCmd;
				char **outputStr;
				outputStr=(char**)malloc(sizeof(char*)*maxCmd);
				for(i=0;i<maxCmd;i++)
					outputStr[i]=(char*)malloc(sizeof(char)*maxCmdSize);
				noCmd=splitCommand((char **)outputStr," ",command_buff);
				if(noCmd==3 && !strncmp(command_buff,"CONNECT",7)){
					struct hostent *host;
					struct sockaddr_in server_addr2;
					if(!isValidIpAddress(outputStr[1])){
						printf("\rNot a valid address!\n");
						printf(">");
						fflush(stdout);
						continue;
					}
					host = gethostbyname(outputStr[1]);
					if ((sock2 = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
						perror("Socket");
						exit(1);
					}
					dest_port=atoi(outputStr[2]);
					server_addr2.sin_family = AF_INET;
					server_addr2.sin_port = htons(dest_port);
					server_addr2.sin_addr = *((struct in_addr *)host->h_addr);
					bzero(&(server_addr2.sin_zero),8);
					if (connect(sock2, (struct sockaddr *)&server_addr2,sizeof(struct sockaddr)) == -1){
						perror("Connect");
						exit(1);
					}
					else{
						currentSock=&sock2;
						printf("Connection successfully estabilished with %s:%d\n",
								inet_ntoa(server_addr2.sin_addr),dest_port);
						printf(">");
							fflush(stdout);
					}
					setNonblocking(sock2);
					fflush(stdout);
					while(1){
						if((bytes_recieved=recv(sock2,recv_data,1024,0))!=-1){
							recv_data[bytes_recieved] = '\0';
							lastCommand=pushCommand(commandHistory,recv_data);
							if (strcmp(recv_data , "q") == 0 || strcmp(recv_data , "Q") == 0){
								printf("\r%s:%d closed connection with you", 
									inet_ntoa(server_addr2.sin_addr),dest_port);
								printf("\n>");
								fflush(stdout);
								close(sock2);
								break;
							}
							else{
								printf("\rCommand recieved = %s" , recv_data);
							fflush(stdout);
								cmdParser(sock2, recv_data);
								printf("\n>");
								fflush(stdout);
							}
						}
						else if(waitForStdin(1)){
							gets(send_data);
							lastCommand=pushCommand(commandHistory,send_data);
							if (strcmp(send_data , "q") != 0 && strcmp(send_data , "Q") != 0){
								handleRequest(sock2, send_data);
								memset(send_data,'\0',1024);
								printf(">");
								fflush(stdout);
							}
							else{
								send(sock2,send_data,strlen(send_data), 0);
								printf("\rYou closed connection with %s:%d",
									inet_ntoa(server_addr2.sin_addr), dest_port);
								memset(send_data,'\0',1024);
								printf("\n>");
								fflush(stdout);
								close(sock2);
								break;
							}
						}
					}
				}
				else{
					cmdParser(sock2, command_buff);
				}
			}
		}
	}       
	close(sock);
	return 0;
} 
