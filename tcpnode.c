#include "tcpnode.h"

/*
Used for creating error messages and terminating program
on errors.
*/
void oops(char *err){

	perror(err);
	exit(-1);
}
/*
Checks if the arguments of the app are correct.
*/
void checkArgs(int argc, char *argv[]){

	if(argc < 3){
		fprintf(stderr, "Usage: %s local-port next-host next-port\n", argv[0]);
		exit(-1);
	}
}

pthread_t workers[2];
pthread_mutex_t lock;

int main(int argc, char *argv[]){

	checkArgs(argc,argv);
	if(pthread_mutex_init(&lock,NULL) != 0) oops("mutex lock init fail: ");

	int err1,err2;
	config *info;
	info = calloc(1,sizeof(config));

	info = initConfig(info,argv);

	err1 = pthread_create( &(workers[0]), NULL, &serverThread, (void*)info);
	err2 = pthread_create( &(workers[1]), NULL, &clientThread, (void*)info);

	if(err1 != 0) oops("Thread1 failed: ");
	if(err2 != 0) oops("Thread2 failed: ");

	pthread_join(workers[0],NULL);
	pthread_join(workers[1],NULL);

	free(info->port1);
	free(info->port2);
	free(info->hostname);
	free(info->buff);
	free(info->rtid);
	free(info);

	return 0;
}

/*
*Return: returns a initialized config struct.
*Usage: Must allocate memory for the struct before calling on this init.
*		Basically only used to init the values of the struct and allocated
*		memory.
*/
config *initConfig(config *conf, char *argv[]){
	
	config *info;
	info = conf;

	info->port1 = calloc(1,15);
	info->port2 = calloc(1,15);
	info->hostname = calloc(1,25);
	info->buff = malloc(BUFF_SIZE);
	info->rtid = malloc(BUFF_SIZE);
	
	bzero(info->buff,BUFF_SIZE);
	bzero(info->rtid,BUFF_SIZE);

	strcpy(info->port1,argv[1]);
	strcpy(info->port2,argv[3]);
	strcpy(info->hostname, argv[2]);

	info->tid = pthread_self();
	info->send = true;
	info->leader = false;
	info->done = false;
	info->quit = false;

	char tmp[BUFF_SIZE];
	sprintf(tmp,"%ld",info->tid);
	strcpy(info->buff,tmp);
	
	
	return info;
}


/*
*Return: nothing
*Usage: The function that the thread that runs the client calls on.
*Creates a client and runs all the necessary functions to decide the leader.
*
*/
void *clientThread(void *config){

	struct config *info = config;
	int sfd = clientInit(info); //sfd = socket fd

	if(sfd == -1){
		return NULL;
	}
	
	clientAlgorithm(sfd, info);
	redirectCli(info,sfd);
	
	close(sfd);

	return NULL;
}
/*
*Return: nothing
*Usage: The function that the thread that runs the server calls on.
*Creates a server and runs all the necessary functions to decide the leader.
*
*/
void *serverThread(void *config){

	struct config *info = config;
	int clifd = serverInit(info);

	if(clifd == -1){
		return NULL;
	}

	serverAlgorithm(clifd,info);
	redirectServ(info,clifd);

	return NULL;
}

/*
*Return: nothing
*Usage: Read from a socket into a buffer buff. If nothing is sent in 3 sec
*a time out is issued.
*/
void readMessage(int sockfd, char buff[]){
	int bytes;     

	if ( (bytes = recv(sockfd,buff,BUFF_SIZE,0) ) == -1){
			perror("recv err");
			
	}
	else if(bytes == 0){
		//nothing sent, connection lost?\\closed?
		exit(-1);
	}
}

/*
*Return: nothing
*Usage: sends a message to a socket in another computer with the message
*which is in the buffer buff.
*
*/
void sendMessage(int sockfd,char buff[]){

	if(send(sockfd, buff, BUFF_SIZE, 0) == -1){
		perror("send error");
		
	}
}
/*
Return: file description for client socket.
Usage: Sets up a client type socket for
 a thread and tries to connect to an host
Input: struct config *var which is used for all thread functions.
*/
int clientInit(void *config){

	struct config *info = config;
	int sfd,rv;
	int not_connected = true;

	struct addrinfo hints; //struct containing socket info/settings
	struct addrinfo *result, *rp; //to save info of other sockets to be
								  //connected to.
	//before storing information in hints we need to alloc memory for it.
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;     //allows IPv4 and IPv6
	hints.ai_socktype = SOCK_STREAM; //TCP socket
	hints.ai_protocol = 0;    //TCP protocol
	hints.ai_addr = NULL;
	hints.ai_next = NULL;
	hints.ai_canonname = NULL;
	hints.ai_flags = AI_PASSIVE; //For any ip address to be able to bind

	rv= getaddrinfo(info->hostname, info->port2, &hints, &result);
	if(rv != 0){
		fprintf(stderr,"getaddrinfo error %s\n", gai_strerror(rv));
		return -1;
	}

	while(not_connected){ //do this loop untill connected

		for(rp = result; rp != NULL; rp = rp->ai_next){
			sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

			if(sfd == -1){
				perror("Socket fail");
				return -1;
			}
			if(connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1){
				not_connected = false;
				break;   		/*Successfull connection!*/
				
			}
			close(sfd); //else close socket and try another connection
		}
		sleep(1);
	}

	if(rp == NULL){ //no address structures were found
		fprintf(stderr,"No connection, exiting client thread...\n");
		return -1;
	}
	
	freeaddrinfo(result); //No longer need result variable.
	//if we get here without return NULL it means connection estb.
	return sfd;
}

/*
Return: file descriptor for server socket.
Usage: Sets up a server type socket for a thread to wait for connections.
Input: struct config *var which is used for all thread functions.
*/
int serverInit(void *config){

	int serverfd,clilen, iPort, clifd,rv; //create vars
	struct config *info = config;

	iPort = atoi(info->port1);

	struct sockaddr_in cli_addr; // one socket connected to this node
							   	// other socket is programs own.
	struct sockaddr_in serv_addr; //to save info of server address
	struct addrinfo hints; //struct containing socket info/settings
	struct addrinfo *result, *rp; //to save info of other sockets to be
								  //connected to.
	//before storing information in hints we need to alloc memory for it.

	memset(&hints, 0, sizeof(hints));

	hints.ai_family = AF_UNSPEC;     //allows IPv4 and IPv6
	hints.ai_socktype = SOCK_STREAM; //TCP socket
	hints.ai_protocol = 0; //TCP protocol
	hints.ai_addr = NULL;
	hints.ai_next = NULL;
	hints.ai_canonname = NULL;
	hints.ai_flags = AI_PASSIVE; //For any ip address to be able to bind

	
	rv = getaddrinfo(NULL, info->port1, &hints, &result);
	if(rv != 0){
		fprintf(stderr,"getaddrinfo error %s\n", gai_strerror(rv));
		exit(-1);
		return -1;
	}
	
	


	bzero( (char *)&serv_addr, sizeof(serv_addr) ); 

	/*zeroes the buffer "serv_addr" to create serv addr.
	//set value of serv_addr types :
	1) family = what type of domain 
	2) my computers ip addr 
	3) portnumber in network byte order
	*/
	serv_addr.sin_family = AF_INET; //domain = internet
	serv_addr.sin_addr.s_addr = INADDR_ANY; //currnet box IP set by INADDR_ANY
	serv_addr.sin_port = htons(iPort); //converts host byte order to network b.o
 
	clilen = sizeof(cli_addr);

	for(rp = result; rp != NULL; rp = rp->ai_next) {

		serverfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		
		if(serverfd == -1) continue;
		
		if(bind(serverfd,rp->ai_addr,rp->ai_addrlen) == 0){
			break; //Bind succesfull
		}
		close(serverfd);
	}
	
	if(rp == NULL) {
		perror("Could not bind");
		return -1;
	}

	int optval = 1;
	setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));


	listen(serverfd,2); //listen for clients to connect, maximum 1
					// connections to be queued during connection

	freeaddrinfo(result); //clear after done using
	
	clifd = accept(serverfd,(struct sockaddr *)&cli_addr,(socklen_t *)&clilen);
		if(clifd < 0){
			fprintf(stderr,"Error on accepting connection\n");
			return -1;
		}


	struct timeval timeout; 
    timeout.tv_sec = 3; // 3 seconds timeout-timer
    timeout.tv_usec = 0;
   
	/*TCP => connection estb. to be able to read,
	there fore setting sockopt is ok now.*/
	//change settings of socket to time out incase of delayed msg
    
	if( setsockopt(serverfd,SOL_SOCKET,SO_RCVTIMEO, (char *)&timeout,
														sizeof(timeout)) < 0){
		perror("setsockopt err");
		return -1;
	}

	if(clifd < 0){
		perror("socket error");
		return -1;

	}
	close(serverfd); //closeing because we only expect 1 connection and
			//dont need to be listening any more for connections

	return clifd;
}

/*
*ONLY TO BE CALLED FROM SERVER THREAD.
*Return: 1 on finish, but not used for anything.
*Usage: To establish the leading node to send messages around the ring.
*ONLY TO BE CALLED FROM SERVER THREAD.
*/
int serverAlgorithm(int sockfd,void *config){
	struct config *info = config;
	int rv = 452; // this will stop from entering with
				  // random number to if statement
	int stop = false;

	while(1){

		readMessage(sockfd,info->rtid);//read sent msg into cmpBuff

		rv = cmpID(info->tid,info->rtid);
		stop = checkForStop(info->rtid);

		if(stop){
			pthread_mutex_lock(&lock);
			info->done = true;
			bzero(info->buff,BUFF_SIZE);
			char tmp[BUFF_SIZE];
			sprintf(tmp,"stop");
			strcpy(info->buff,tmp);
			info->quit = true;
			pthread_mutex_unlock(&lock);
			return 1;
		}
	
		if(!rv){
			//msg did full rotation and im the master node
			printf("*********************************\n");
			printf("********I AM THE LEADER!*********\n");
			printf("*********************************\n");
			pthread_mutex_lock(&lock);
			info->leader = true;
			info->done = true;
			info->send = false;
			bzero(info->buff,BUFF_SIZE);
			char tmp[BUFF_SIZE];
			sprintf(tmp,"stop");
			strcpy(info->buff,tmp);
			pthread_mutex_unlock(&lock);
		}
		else if(rv < 0){ //recieved id > current id

			pthread_mutex_lock(&lock);
			info->send = true;
			bzero(info->buff,BUFF_SIZE);
			strcpy(info->buff,info->rtid);
			pthread_mutex_unlock(&lock);
		}
		bzero(info->rtid,BUFF_SIZE);
	}
}

/*
*ONLY TO BE CALLED FROM CLIENT THREAD.
*Return: 1 on finish, but not used for anything.
*Usage: To establish the leading node to send messages around the ring.
*ONLY TO BE CALLED FROM CLIENT THREAD.
*/
int clientAlgorithm(int sockfd, void *config){

	struct config *info = config;

	struct timespec ts;
	ts.tv_sec = 4 / 1000;
	ts.tv_nsec = (4 % 1000) * 1000000; // 4ms


	while(1){

		if(info->done){
			//tell every node to stop the alg.
			pthread_mutex_lock(&lock);
			sendMessage(sockfd,info->buff);
			pthread_mutex_unlock(&lock);

			while(1)
			{
				if(!info->quit)
				{
					nanosleep(&ts, NULL);
					continue;
				}

				return 1;
			}
		} 

		if(info->send){
			
			pthread_mutex_lock(&lock);
			sendMessage(sockfd,info->buff);
			info->send = false;
			pthread_mutex_unlock(&lock);
		}
	}
			
}

/*
Return: returns true if thread id matches recieved thread id.
Usage: to compare 2 large ints using strings instead of int or long int
		in case of int overflow
*/
int cmpID(long int tid, char *rtid){

	char tmp[BUFF_SIZE];
	sprintf(tmp,"%ld",tid);

	int rv = strcmp(tmp,rtid);
	
	return rv;
	
}

/*
Return: True on match to stop, else false
Checks for stop message being sent. If stop is sent
return true, else false.
*/
int checkForStop(char *str){

	int rv = strcmp(str,"stop");

	return (rv == 0);
}
/*
*Return: nothing
*Usage: This function only redirects messages recieved. A node will send a
*		message and the server will read it and place it in the buffer for
*		the client to use and send.
*/
void redirectServ(void *conf,int sockfd){
	//
    config *info = conf;
	struct pollfd fd;
	int res;

    pthread_mutex_lock(&lock);
	fd.fd = sockfd;
	fd.events = POLLIN;
	pthread_mutex_unlock(&lock);

	while(1)
	{
		res = poll(&fd, 1, 1); //1 ms timeout

		if( res == -1)
		{
			perror("Poll");
		}
		else if(res != 0)	//If res is not -1 or 0 we can read recv
		{
			pthread_mutex_lock(&lock);
			bzero(info->buff,BUFF_SIZE);
			readMessage(sockfd, info->buff);
			info->send = true;
			int i = atoi(info->buff);
			i++;
			bzero(info->buff, BUFF_SIZE);
			char tmp[BUFF_SIZE];
			sprintf(tmp, "%d", i);
			strcpy(info->buff, tmp);
			pthread_mutex_unlock(&lock);
		}
	}
}

/*
*Return: nothing
*Usage: The last function to run in the program which if the node is a leader,
*	    sends an 1 which keeps being added to in the ring. Every 10.000 rev.
*		the time is taken and logged in a log-file to keep track of sendspeed.
*/
void redirectCli(void *conf,int sockfd){

	config *info = conf;
	int i = 1;
	int logNr = 1;
	struct timeval start,end;

	FILE *fp;
	pthread_mutex_lock(&lock);
	info->send = false;
	pthread_mutex_unlock(&lock);

	pthread_mutex_lock(&lock);	
	if(info->leader){
		
		fp = fopen("time-log.log", "a+");

		bzero(info->buff,BUFF_SIZE); //fills the buffer with null-signs
		char tmp[BUFF_SIZE];
		sprintf(tmp,"%d",i); //fill the buff with 1 and sends it, on the other
		strcpy(info->buff,tmp); //nodes receiving end, add 1 to this message
		info->send = false;		//and send it again.
		sendMessage(sockfd,info->buff);
		
		gettimeofday(&start,NULL); //gets the time
		i++;
	}
	pthread_mutex_unlock(&lock);

	while(1){
		pthread_mutex_lock(&lock);
		if(info->send){
			if(info->leader && (i == 10000) ) {
				gettimeofday(&end,NULL); //stops the time

				double sec = (end.tv_sec - start.tv_sec) + 
                ((end.tv_usec - start.tv_usec)/1000000.0); //calculates the time
								//it takes to send 10000 messages in the ring
            	double avg = sec/10000;
				
				printf("%.2f s for 10k and average %f s\n",sec,avg);

				fprintf(fp,
					"log  %d: %.2f ms for 10k revs and average is %f\n"
															,logNr,sec,avg);
				fflush(fp);
				logNr++;
				i = 0;
				gettimeofday(&start,NULL); //gets the time
			}
			
			info->send = false;
			sendMessage(sockfd,info->buff);
			i++;	
		}
		pthread_mutex_unlock(&lock);
	}
}

/*
*Return: Nothing
*Not used at the moment, but was intended to set every empty buffer slot with
*a character a. Was replaced by bzero which sets \0 to every slot of the buffer.
*/
void fillBuffer(char *buff){

	bzero(buff,BUFF_SIZE);
	int i;
	char tmp[BUFF_SIZE];
	for(i = 0; i < BUFF_SIZE; i++){

		tmp[i] = 'a';
	}
	strcpy(buff,tmp);
}
