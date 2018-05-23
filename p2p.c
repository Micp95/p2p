#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h> 
#include <time.h>

#define MY_POEM_LINE "Test 1"
#define MY_POEM_NUM 2

#define PORT 8080
#define ADDRESS_MASK "172.17.0.%d"

#define NUM_THREADS 100

/******************************/
/******************************/
/***    Global variables    ***/
/***shared for all threads! ***/
/******************************/
/******************************/

//static data
char myLineStr[256] = MY_POEM_LINE;
int myLineNumber = MY_POEM_NUM;
static pthread_mutex_t cs_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
FILE *logFile;



//critical data
int	poemLineNumbers[30];
int poemLineCounter = 0;
char* poemLine[30];



void logMessage(char const * mg);
void bPoemSort();


/******************************/
/******************************/
/***Helper functions- client***/
/******************************/
/******************************/


int connectionToServer(int const socket, char const *ip){
	struct sockaddr_in serv_addr;
	memset(&serv_addr, '0', sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	
	if(inet_pton(AF_INET, ip, &serv_addr.sin_addr)<=0) {
		return -1;
	}
	
	if (connect(socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
		return -1;
	}
	
	char str[128];
  sprintf(str,"CLIENT - Successfully connection to server: %s", ip);
	logMessage(str);
	return 0;
}

int createConnectionSocket (){
	int sock = 0;
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		  logMessage("ERROR CLIENT - creating connection socket");
		  return -1;
	}
	return sock;
}


/******************************/
/******************************/
/***Helper functions- server***/
/******************************/
/******************************/

int createServerSocket(struct sockaddr_in *address){
	int server_fd;
	int opt = 1;

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0){
	  logMessage("ERROR SERVER - creating server socket: socket");
	  return -1;
  }
  
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))){
		logMessage("ERROR SERVER - creating server socket: setsockopt");
		close(server_fd);
		return -1;
  }
  
  if (bind(server_fd, (struct sockaddr *)address,sizeof(*address))<0){
    logMessage("ERROR SERVER - creating server socket: bind");
    close(server_fd);
    return -1;
  }
  
  if (listen(server_fd, 10) < 0){
    logMessage("ERROR SERVER - creating server socket: listen");
    close(server_fd);
    return -1;
  }
  
  return server_fd;
}

void setServerAddress(struct sockaddr_in *address){
    address->sin_family = AF_INET;
    address->sin_addr.s_addr = INADDR_ANY;
    address->sin_port = htons( PORT );
}

void logClientIp(int new_socket){

  struct sockaddr_in addr;
  socklen_t addr_size = sizeof(struct sockaddr_in);
  int res = getpeername(new_socket, (struct sockaddr *)&addr, &addr_size);
  char *clientip = (char*)malloc(20);
  inet_ntop(AF_INET, &addr.sin_addr, clientip, 20);
  
	char str[128];
 	sprintf(str,"SERVER - Successfully connection with client (%d): %s",new_socket, clientip);
	logMessage(str);
	free(clientip);
}

int waitingForClient (int const socket,struct sockaddr_in *address){
	int new_socket;
	int addrlen = sizeof(*address);
	
	if ((new_socket = accept(socket, (struct sockaddr *)&address,(socklen_t*)&addrlen))<0){
			logMessage("ERROR SERVER - creating server socket: listen");
		  return -1;
	}
	
	logClientIp(new_socket);
	return new_socket;
}


/******************************/
/******************************/
/***Helper functions - send ***/
/******************************/
/******************************/


int sendNumber(int const socket, int nubmer){
	int32_t numberToSend = (int32_t)nubmer;
	int res = send(socket , &numberToSend , sizeof(int32_t) , 0 );
	if(res == -1 || res != sizeof(int32_t)){
		logMessage("ERROR - sending number");
		return -1;
	}
	return 0;
}

int sendStringArr (int const socket, char const* stringToSend, int size){

	if(sendNumber(socket,size) != 0){
		return -1;
	}
	int sentCount =0;
	int res;
	
	while (sentCount < size ){
		res = send(socket , stringToSend+sentCount , size-sentCount , 0);
		if(res == -1){
			logMessage("ERROR - sending string");
			return -1;
		}
		sentCount += res;
	}
	return 0;
}

int sendStatement(int const socket, int flag){
	char respYes[3] = "yes";
	char respNo[3] = "no";
	int res;
	
	if(flag == 1)
		res = send(socket , respYes , 3 , 0);
	else 
		res = send(socket , respNo , 3 , 0);
	
	if(res == -1 || res != 3){
		logMessage("ERROR - sending short statement");
		return -1;
	}
	
	return 0;
}


/******************************/
/******************************/
/***Helper functions - read ***/
/******************************/
/******************************/

int getNumber (int const socket){
	int32_t numberToRead;
	if (read(socket ,&numberToRead, sizeof(int32_t)) == -1){
		logMessage("ERROR - get number");
		return -1;
	}
	return (int)numberToRead;
}

int getStringArr(int const socket, char **readString){

	int size =getNumber(socket); 
	if( size == -1){
		return -1;
	}
	
	(*readString) = (char *)malloc(size+1);
	int readedLen = 0;
	int err;
	
	while ( readedLen < size){
		err = read(socket , (*readString)+readedLen,size-readedLen );
		if(err == -1){
			logMessage("ERROR - get string");
			return -1;
		}
		readedLen += err;
	}
	
	(*readString)[size]='\0';
	return 0;
}

int readStatement(int const socket, int* flag){
	char buff[3];
	if(read(socket,buff,3) != 3){
		logMessage("ERROR - get statement - invalid length");
		return -1;
	}
	
	if(buff[0] == 'y' || buff[0] == 'Y' )
		(*flag) =1;
	else
		(*flag) =0;
		
	return 0;
}





/******************************/
/******************************/
/***     Client SECTION     ***/
/******************************/
/******************************/

//method for validate poem line - checking if we already have line in memory
int validationPoemLine(int number, int* numbers, int size){
	int k;
	for (k=0;k<size;k++){
		if(numbers[k] == number)
			return 0;
		else if(numbers[k] == -1) //default array are initialization by -1 integers
			return 1;
	}
	return 0; 
}



//function for connection with server
//ipNum_v - is last part of server ip to connection (192.168.10.x)
void * ClientThreadConnect(void * ipNum_v){
	
	int currentIp = *(int*)ipNum_v;
	int socket;
	int poemLineNumber;
	int error;
	char ip[20];
	char *thLine;
	char str[128];

	//build address IP
	snprintf(ip,sizeof(ip),ADDRESS_MASK,currentIp);
	
	//create socket for connection
	socket = createConnectionSocket();
	
	//try to connection
	error = connectionToServer(socket,ip);

	if ( error != -1){
		
		//get line poem number from server
		poemLineNumber = getNumber(socket);
		sprintf(str,"CLIENT -%s- Get line number from server: %d",ip, poemLineNumber);
		logMessage(str);
		
		//poemLineNumber++; //line for debugging
		
		//validate line number
		if(validationPoemLine(poemLineNumber,poemLineNumbers,30) == 1){

			//send to server "yes" message
			sendStatement(socket,1);
			
			//get from server poem line
			getStringArr(socket,&(thLine));
			
			sprintf(str,"CLIENT -%s- Get new line: %s",ip,thLine);
			logMessage(str);
			
			//CRITICAL SECTION START
			pthread_mutex_lock(&cs_mutex);
			
			//save information in memory
			poemLine[poemLineCounter] = thLine;
			poemLineNumbers[poemLineCounter] = poemLineNumber;
			poemLineCounter++;
			
			bPoemSort();
			
			if( poemLineCounter >= 30)
				poemLineCounter =0;
				
			pthread_mutex_unlock(&cs_mutex);
			//CRITICAL SECTION END
				
			
		}else{
			//send to server "no" message
			sendStatement(socket,0);
			
			sprintf(str,"CLIENT -%s- Duplicate line number",ip);
			logMessage(str);
		}
	}
	
	//close socket and tread
	close(socket);
	pthread_exit(NULL);
}


//function for scanning local network
void ClientThreadScan(){

	//variables for threads
	pthread_t thread[NUM_THREADS];
	int threadArgs[NUM_THREADS];
	int currentIp = 0;

	while(1){
	
		//decomposition ip's for threads
		for(int th = 0; th < NUM_THREADS; th++){
			threadArgs[th] = currentIp;
			pthread_create(&thread[th], NULL, ClientThreadConnect, (void *)&threadArgs[th]);
		
			currentIp++;
			if(currentIp >= 254)
				currentIp=0;
		}
		
		//waiting for threads execution
		for(int th = 0; th < NUM_THREADS; th++)
			pthread_join(thread[th],NULL);
	}
	
	//close tread
	pthread_exit(NULL);
}

/******************************/
/******************************/
/***     Server SECTION     ***/
/******************************/
/******************************/

//function to communication with client
void * ServerSendLine(void * socket_v){
	int socket = *(int*)socket_v;
	int statement;
	char str[128];
	
	//send my poem line number
	sendNumber(socket,myLineNumber);
	sprintf(str,"SERVER - Successfully sent line number to client (%d)",socket);
	//check client response
	readStatement(socket,&statement);
	
	//if client sent - "yes" - send poem line
	if( statement == 1){
		sendStringArr(socket,myLineStr,strlen(myLineStr));
		sprintf(str,"SERVER - Successfully sent poem line to client (%d)",socket);
		logMessage(str);
	}
	
	//close socket and tread
	close(socket);
	sprintf(str,"SERVER - Successfully closed connection to client (%d)",socket);
	logMessage(str);
	pthread_exit(NULL);
}

//main server function
void* ServerThread(){
	//generate address for socket
	struct sockaddr_in address;
	setServerAddress(&address);
	
	//create socket for server
	int socket = createServerSocket(&address);
	int clientSocket =-1;
	
	//variables for threads
	pthread_t thread[NUM_THREADS];
	int threadArgs[NUM_THREADS];
	int th_count = 0;
	
	//infinity loop for connections with clients
	while(1){
		//try to connect with client
		clientSocket = waitingForClient(socket,&address);
		
		//if success - create thread for connection service
		if(clientSocket != -1){
			//prepare a parameter for thread and create thread
			threadArgs[th_count] = clientSocket;
			pthread_create(&thread[th_count], NULL, ServerSendLine, (void *)&threadArgs[th_count]);
			
			th_count++;
			if( th_count >= NUM_THREADS)
				th_count =0;
		}
	}
	pthread_exit(NULL);
}



/******************************/
/******************************/
/***      MAIN SECTION      ***/
/******************************/
/******************************/


//print poem in console
void showPoem(){
	printf("\033[H\033[J"); //clear console
	
	printf("--POEM--\n\n\n");
	for(int k =0;k<30;k++){
		if(poemLineNumbers[k] == -1)
			break;
		printf("%d: %s \n",poemLineNumbers[k],poemLine[k]);
	}
	printf("\n\n--END--\n\n\n");
	
}

//initialize poem array's
void initMyPoemPart(){
	int k;
	for(k=1; k< 30;k++)
		poemLineNumbers[k] = -1;
	poemLineNumbers[poemLineCounter] =MY_POEM_NUM;
	poemLine[poemLineCounter++] = myLineStr;
}


void bPoemSort(){
	int tempNumber;
	char* tempPointer;
	int k,endFlag;
	
	do{
		endFlag =0;
		k = poemLineCounter-1;
		do{
			k--;
			if(poemLineNumbers[k+1] < poemLineNumbers[k]){
				tempNumber = poemLineNumbers[k];				
				poemLineNumbers[k] = poemLineNumbers[k+1];
				poemLineNumbers[k+1] = tempNumber;
				
				tempPointer = poemLine[k];
				poemLine[k] = poemLine[k+1];
				poemLine[k+1] = tempPointer;
				
				endFlag = 1;
			}
		}while(k !=0);
	}while(endFlag !=0);
}


void logMessage(char const * mg){
	pthread_mutex_lock(&log_mutex);
	
	logFile = fopen("logs.txt", "a");
	
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);

	fprintf(logFile,"--EventTime: %d-%d-%d %d:%d:%d\t", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	fprintf(logFile, "%s\n",mg);
	
	fclose(logFile);
	
	pthread_mutex_unlock(&log_mutex);
}

//main function
int main (){
	initMyPoemPart();
	
	logMessage("Start Working");

	//two threads from algorithm
	pthread_t thread[2];
	
	pthread_create(&thread[0], NULL, (void *)ClientThreadScan, NULL);
	pthread_create(&thread[1], NULL, (void *)ServerThread, NULL);
	
	char ch;
	//main loop for print poem
	while(1){
		showPoem();
		sleep(1);
	}
	
	
	logMessage("End Working");
	pthread_exit(NULL);
}






