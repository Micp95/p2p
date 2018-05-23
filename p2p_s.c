#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h> 

#define PORT 8080
#define MY_POEM "TEST 1"
#define MY_POEM_NUM 9
#define NUM_THREADS 100

//Client section
int connectionToServer(int const socket, char const *ip){
	struct sockaddr_in serv_addr;
	memset(&serv_addr, '0', sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	
	if(inet_pton(AF_INET, ip, &serv_addr.sin_addr)<=0) {
		//printf("\nInvalid address/ Address not supported \n");
		return -1;
	}
	
	if (connect(socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
		//printf("\nConnection Failed \n");
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


//Server section
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
		  printf("accept");
		  return -1;
	}
	return new_socket;
}


//Send section
int sendNumber(int const socket, int nubmer){
	int32_t numberToSend = (int32_t)nubmer;
	int res = send(socket , &numberToSend , sizeof(int32_t) , 0 );
	if(res == -1 || res != sizeof(int32_t)){
		printf("\n Send number error \n");
		return -1;
	}
	return 0;
}

int sendStringArr (int const socket, char const* stringToSend, int size){

	if(sendNumber(socket,size) != 0){
		printf("\n Send size error \n");
		return -1;
	}
	int sentCount =0;
	int res;
	
	while (sentCount < size ){
		res = send(socket , stringToSend+sentCount , size-sentCount , 0);
		if(res == -1){
			printf("\n Send number error \n");
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


//Read section

int getNumber (int const socket, int* number){
	int32_t numberToRead;
	
	if (read(socket , &numberToRead, (int32_t)numberToRead) == -1){
		printf("\n read number error \n");
		return -1;
	}
	*number = (int)numberToRead;
}

int getStringArr(int const socket, char *readString){
	int size;
	if(getNumber(socket,&size) != 0){
		printf("\n read size error \n");
		return -1;
	}
	readString = malloc(size);
	int readedLen = 0;
	int err;
	while ( readedLen < size){
		err = read(socket , readString+readedLen,size-readedLen );
		if(err == -1){
			printf("\n read error \n");
			return -1;
		}
		readedLen += err;
	}
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

//Log section



char myLineStr[256] = MY_POEM;
int myLineNumber = MY_POEM_NUM;

//MAIN file

//Algorithm section

int validationPoemLine(int number, int* numbers, int size){
	int k;
	for (k=0;k<size;k++){
		if(numbers[k] == number)
			return 0;
		else if(numbers[k] == -1)
			return 1;
	}
	return 0; 
}

void ClientThread(){

	int socket = createConnectionSocket();
	if(socket == -1){
		//exit
	}
		
	int k;
	char ip[20];
	char* poemLine[30];
	int	poemLineNumbers[30];
	int poemLineCounter = 0;


	int poemLineNumber;
	int currentIp = 0;
	int error;
	
	for(k=1; k< 30;k++)
		poemLineNumbers[k] = -1;
	poemLineNumbers[poemLineCounter] =MY_POEM_NUM;
	poemLine[poemLineCounter++] = myLineStr;
	
	
	while(1){
	
		snprintf(ip,sizeof(ip),"172.17.0.%d",currentIp);
		
		socket = createConnectionSocket();
		error = connectionToServer(socket,ip);
		if ( error != -1){
			printf("client connected\n");
			getNumber(socket,&poemLineNumber);
			
			if(validationPoemLine(poemLineNumber,poemLineNumbers,30) == 1){
				sendStatement(socket,1);
				getStringArr(socket,poemLine[poemLineCounter]);
				poemLineNumbers[poemLineCounter++] = poemLineNumber;
				
				printf("\n\nGet poem:%d\t%s",poemLineNumber,poemLine[poemLineCounter-1]);
			}else{
				sendStatement(socket,0);
			}
			
		}
		close(socket);

		currentIp++;
		if(currentIp >= 255)
			currentIp=0;
	}
	
	pthread_exit(NULL);
}

void * ServerSendLine(void * socket_v){
	int socket = *(int*)socket_v;
	int statement;
	
	sendNumber(socket,myLineNumber);
	readStatement(socket,&statement);
	if( statement == 1){
		sendStringArr(socket,myLineStr,strlen(myLineStr));
	}
	
	close(socket);
	pthread_exit(NULL);
}

void* ServerThread(){
	struct sockaddr_in address;
	setServerAddress(&address);
	
	int socket = createServerSocket(&address);
	int clientSocket =-1;
	pthread_t thread[NUM_THREADS];
	int th_count = 0;
	printf ("%d\n",socket);
	while(1){
		clientSocket = waitingForClient(socket,&address);
		if(clientSocket != -1){

			pthread_create(&thread[th_count++], NULL, ServerSendLine, (void *)&clientSocket);
			
			if( th_count >= NUM_THREADS)
				th_count =0;
		}
	}
	pthread_exit(NULL);
}


void showPoem(){


}

int main (){

	pthread_t thread[2];
	
	//pthread_create(&thread[0], NULL, (void *)ClientThread, NULL);
	pthread_create(&thread[1], NULL, (void *)ServerThread, NULL);
	


	while(1){
		showPoem();
		sleep(100);
	}
	

	pthread_exit(NULL);
}






