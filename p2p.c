#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h> 

#define MY_POEM_LINE "TEST 1"
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

//critical data
int	poemLineNumbers[30];
int poemLineCounter = 0;
char* poemLine[30];



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
	
	return 0;
}

int createConnectionSocket (){
	int sock = 0;
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		  printf("\n Socket creation error \n");
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
	  printf("socket failed");
	  return -1;
  }
  
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))){
		printf("setsockopt");
		close(server_fd);
		return -1;
  }
  
  if (bind(server_fd, (struct sockaddr *)address,sizeof(*address))<0){
    printf("bind failed");
    close(server_fd);
    return -1;
  }
  
  if (listen(server_fd, 10) < 0){
    printf("listen failed");
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

int waitingForClient (int const socket,struct sockaddr_in *address){
	int new_socket;
	int addrlen = sizeof(*address);
	
	if ((new_socket = accept(socket, (struct sockaddr *)&address,(socklen_t*)&addrlen))<0){
		  return -1;
	}
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
			return -1;
		}
		readedLen += err;
	}
	
	return 0;
}

int readStatement(int const socket, int* flag){
	char buff[3];
	if(read(socket,buff,3) != 3)
		return -1;
	
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
	
	//build address IP
	snprintf(ip,sizeof(ip),ADDRESS_MASK,currentIp);
	
	//create socket for connection
	socket = createConnectionSocket();
	
	//try to connection
	error = connectionToServer(socket,ip);

	if ( error != -1){
		
		//get line poem number from server
		poemLineNumber = getNumber(socket);
		
		//poemLineNumber++; //line for debugging
		
		//validate line number
		if(validationPoemLine(poemLineNumber,poemLineNumbers,30) == 1){

			//send to server "yes" message
			sendStatement(socket,1);
			
			//get from server poem line
			getStringArr(socket,&(thLine));

			//CRITICAL SECTION START
			pthread_mutex_lock(&cs_mutex);
			
			//save information in memory
			poemLine[poemLineCounter] = thLine;
			poemLineNumbers[poemLineCounter] = poemLineNumber;
			poemLineCounter++;
			
			if( poemLineCounter >= 30)
				poemLineCounter =0;
				
			pthread_mutex_unlock(&cs_mutex);
			//CRITICAL SECTION END
				
			
		}else{
			//send to server "no" message
			sendStatement(socket,0);
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
	
	//send my poem line number
	sendNumber(socket,myLineNumber);
	
	//check client response
	readStatement(socket,&statement);
	
	//if client sent - "yes" - send poem line
	if( statement == 1){
		sendStringArr(socket,myLineStr,strlen(myLineStr));
	}
	
	//close socket and tread
	close(socket);
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
	
	printf("--START--\n");
	for(int k =0;k<30;k++){
		if(poemLineNumbers[k] == -1)
			break;
		printf("%d: %s \n",poemLineNumbers[k],poemLine[k]);
	}

}

//initialize poem array's
void initMyPoemPart(){
	int k;
	for(k=1; k< 30;k++)
		poemLineNumbers[k] = -1;
	poemLineNumbers[poemLineCounter] =MY_POEM_NUM;
	poemLine[poemLineCounter++] = myLineStr;
}

//main function
int main (){
	
	initMyPoemPart();

	//two threads from algorithm
	pthread_t thread[2];
	
	pthread_create(&thread[0], NULL, (void *)ClientThreadScan, NULL);
	pthread_create(&thread[1], NULL, (void *)ServerThread, NULL);
	
	//main loop for print poem
	while(1){
		showPoem();
		sleep(1);
	}
	
	pthread_exit(NULL);
}






