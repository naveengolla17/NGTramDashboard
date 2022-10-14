#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ctype.h>

/* 
    The Tram data server (server.py) publishes messages over a custom protocol. 
    
    These messages are either:

    1. Tram Passenger Count updates (MSGTYPE=PASSENGER_COUNT)
    2. Tram Location updates (MSGTYPE=LOCATION)

    It publishes these messages over a continuous byte stream, over TCP.

    Each message begins with a 'MSGTYPE' content, and all messages are made up in the format of [CONTENT_LENGTH][CONTENT]:

    For example, a raw Location update message looks like this:

        7MSGTYPE8LOCATION7TRAM_ID7TRAMABC5VALUE4CITY

        The first byte, '7', is the length of the content 'MSGTYPE'. 
        After the last byte of 'MSGTYPE', you will find another byte, '8'.
        '8' is the length of the next content, 'LOCATION'. 
        After the last byte of 'LOCATION', you will find another byte, '7', the length of the next content 'TRAM_ID', and so on.

        Parsing the stream in this way will yield a message of:

        MSGTYPE => LOCATION
        TRAM_ID => TRAMABC
        VALUE => CITY

        Meaning, this is a location message that tells us TRAMABC is in the CITY.

        Once you encounter a content of 'MSGTYPE' again, this means we are in a new message, and finished parsing the current message

    The task is to read from the TCP socket, and display a realtime updating dashboard all trams (as you will get messages for multiple trams, indicated by TRAM_ID), their current location and passenger count.

    E.g:

        Tram 1:
            Location: Williams Street
            Passenger Count: 50

        Tram 2:
            Location: Flinders Street
            Passenger Count: 22

    To start the server to consume from, please install python, and run python3 server.py 8081

    Feel free to modify the code below, which already implements a TCP socket consumer and dumps the content to a string & byte array
*/

void error(char* msg) {
    perror(msg);
    exit(1);
}

typedef struct Dashboard {
   char *id;
   char *location;
   char *passenger_count;
   struct Tram *next;
}Tram;

Tram *tram_head = NULL;
Tram *tram_current = NULL;
/* Add node to the linked list*/
void Add(char msg, char *tram_id, char *value) 
{   
	size_t len = 0;
	Tram *tram = (struct Tram*) malloc(sizeof(Tram));

	len = strlen(tram_id) + 1;
	tram->id = (char *)malloc(len);
	strcpy(tram->id, tram_id);
	if(msg == 1) // Copy location
	{
		len = strlen(value) + 1;
		tram->location = (char *)malloc(len);
		strcpy(tram->location, value);
		tram->passenger_count = NULL;
	}
	else if(msg == 2) // Copy passenger count
	{
		len = strlen(value) + 1;
		tram->passenger_count = (char *)malloc(len);
		strcpy(tram->passenger_count, value);
		tram->location = NULL;
	}	
	
	tram->next = NULL;

	// Create new list if head is empty
	if(tram_head==NULL) {
		tram_head = tram;
		return;
	}

	tram_current = tram_head;
   
	// Traverse till end of the list
	while(tram_current->next!=NULL)
		tram_current = tram_current->next;
   
	// Add node at the end of the list
	tram_current->next = tram; 
}
/* Update node to the linked list*/
void Update(char msg, char *tram_id, char *value) 
{   
   int len = 0;
   
   if(tram_head==NULL) 
   {
      //error("List not initialized");
	  Add(msg, tram_id, value);
      return;
   } 

   tram_current = tram_head;
   while(tram_current->next!=NULL) 
   {
      if(strcmp(tram_current->id, tram_id) == 0)
	  {
		if(msg == 1) // update location
		{
			free(tram_current->location);
			len = strlen(value) + 1;
			tram_current->location = (char *)malloc(len);
			strcpy(tram_current->location, value);
		}
		else if(msg == 2) // update passenger count
		{
			free(tram_current->passenger_count);
			len = strlen(value) + 1;
			tram_current->passenger_count = (char *)malloc(len);
			strcpy(tram_current->passenger_count, value);
		}         
        return;
      }      
      tram_current = tram_current->next;      
   }      
   Add(msg, tram_id, value);
}
void DiaplayDashboard() {
   Tram *tram = tram_head;
   int space = 10;
   system("clear");
   printf("************Tram Dashboard****************\n");
   while(tram != NULL) 
   {
	  if(tram->location && tram->passenger_count)
	  {
		printf("%s :\n",tram->id);
		printf("\tLocation : %s\n", tram->location);
		printf("\tPassenger Count : %s\n", tram->passenger_count);		
	  }
	  tram = tram->next;
   }

   printf("\n");
}
//*MSGTYPE*PASSENGER_COUNT*TRAM_ID*Tram 2*VALUE*65
//*MSGTYPE*LOCATION*TRAM_ID*Tram 4*VALUE*Williams Street

void dump_buffer(char* name) {
    int e, i,j,k;
    size_t len = strlen(name);
	char *buf;
	char *MsgType;
	char *TramId;
	char tram_id[30] = {'\0'};
	char value[50] = {'\0'};
	char TramIdLen = 8;
	char ValueLen = 6;
	char msg = 0; // 1 - Location, 2 - Passenger count
	buf = (char *)malloc(len+1);
	memset(buf, '\0', len+1);
    
    for (size_t i = 0; i < len; i++) {
        char c = name[i];
        if (!isalpha(name[i]) && !isdigit(name[i]) && (c != '_') && (c != ' '))
            c = '*';
        //printf("%-5c", c);
		//printf("%c", c);
		buf[i] = c;
    }	
	//printf("%s\n", buf);
	MsgType = strstr(buf,"PASSENGER_COUNT");
	if(MsgType)	
		msg = 2;
	else
	{
		MsgType = strstr(buf,"LOCATION");
		if(MsgType)
			msg = 1;
		else
		{
			error("Invalid buffer data\n");
			return;
		}
	}
	TramId = strstr(buf,"TRAM_ID");
	i = 0;
	j = 0;
	k = 0;
	if(TramId)
	{
		while(TramId[TramIdLen+i] != '*')
		{
			tram_id[j] = TramId[TramIdLen+i];
			i++;
			j++;
		}
		j = 0;
		i++;
		while(TramId[TramIdLen+i+ValueLen+k] != '\0')
		{
			value[j] = TramId[TramIdLen+i+ValueLen+k];
			k++;
			j++;
		}
		if(strlen(tram_id) > 0 && strlen(value) > 0)
		{
			Update(msg, tram_id, value);
			DiaplayDashboard();	
			printf("\n");
		}
	}	
	
	free(buf);
}

int main(int argc, char *argv[]){
	if(argc < 2){
        fprintf(stderr,"No port provided\n");
        exit(1);
	}	
	int sockfd, portno, n;
	char buffer[255];
	
	struct sockaddr_in serv_addr;
	struct hostent* server;
	
	portno = atoi(argv[1]);
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd<0){
		error("Socket failed \n");
	}
	
	server = gethostbyname("127.0.0.1");
	if(server == NULL){
		error("No such host\n");
	}
	
	bzero((char*) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char*) server->h_addr, (char*) &serv_addr.sin_addr.s_addr, server->h_length);
	serv_addr.sin_port = htons(portno);
	
	if(connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr))<0)
		error("Connection failed\n");
	
	while(1){
		bzero(buffer, 256);
		n = read(sockfd, buffer, 255);
		if(n<0)
			error("Error reading from Server");
		dump_buffer(buffer);
	}
	
	return 0;
}