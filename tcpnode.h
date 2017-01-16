#include <stdio.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <time.h>
#include <fcntl.h>

#define true 1
#define false 0
#define BUFF_SIZE 100
#define REVS 10000


struct config{

	char *port1;
	char *port2;
	char *hostname;
	char *buff;	//100-byte buffers.
	char *rtid;  //100-byte buffers.
	long int tid;
	int send,done,leader,quit;

}typedef config;

/*
*Return: nothing
*Usage: The function that the thread that runs the server calls on.
*Creates a server and runs all the necessary functions to decide the leader.
*
*/
void *serverThread(void *config);
/*
*Return: nothing
*Usage: The function that the thread that runs the client calls on.
*Creates a client and runs all the necessary functions to decide the leader.
*
*/
void *clientThread(void *config);
/*
*Return: nothing
*Usage: Read from a socket into a buffer buff. If nothing is sent in 3 sec
*a time out is issued.
*/
void readMessage(int sockfd, char buff[]);
/*
*Return: nothing
*Usage: sends a message to a socket in another computer with the message
*which is in the buffer buff.
*
*/
void sendMessage(int sfd,char buff[]);
/*
Return: file description for client socket.
Usage: Sets up a client type socket for
 a thread and tries to connect to an host
Input: struct config *var which is used for all thread functions.
*/
int clientInit(void *config);
/*
Return: file descriptor for server socket.
Usage: Sets up a server type socket for a thread to wait for connections.
Input: struct config *var which is used for all thread functions.
*/
int serverInit(void *config);
/*
*ONLY TO BE CALLED FROM SERVER THREAD.
*Return: 1 on finish, but not used for anything.
*Usage: To establish the leading node to send messages around the ring.
*ONLY TO BE CALLED FROM SERVER THREAD.
*/
int serverAlgorithm(int sockfd,void *config);
/*
*ONLY TO BE CALLED FROM CLIENT THREAD.
*Return: 1 on finish, but not used for anything.
*Usage: To establish the leading node to send messages around the ring.
*ONLY TO BE CALLED FROM CLIENT THREAD.
*/
int clientAlgorithm(int sockfd,void *config);
/*
Return: returns true if thread id matches recieved thread id.
Usage: to compare 2 large ints using strings instead of int or long int
		in case of int overflow
*/
int cmpID(long int tid, char *rtid);

/*
*Return: returns a initialized config struct.
*Usage: Must allocate memory for the struct before calling on this init.
*		Basically only used to init the values of the struct and allocated
*		memory.
*/
config *initConfig(config *conf, char *argv[]);
/*
Return: True on match to stop, else false
Checks for stop message being sent. If stop is sent
return true, else false.
*/
int checkForStop(char *str);
/*
*Return: Nothing
*Not used at the moment, but was intended to set every empty buffer slot with
*a character a. Was replaced by bzero which sets \0 to every slot of the buffer.
*/
void fillBuffer(char *buff);
/*
*Return: nothing
*Usage: This function only redirects messages recieved. A node will send a
*		message and the server will read it and place it in the buffer for
*		the client to use and send.
*/
void redirectServ(void *conf,int sockfd);
/*
*Return: nothing
*Usage: The last function to run in the program which if the node is a leader,
*	    sends an 1 which keeps being added to in the ring. Every 10.000 rev.
*		the time is taken and logged in a log-file to keep track of sendspeed.
*/
void redirectCli(void *conf,int sockfd);