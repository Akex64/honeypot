#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <time.h>
#include <pthread.h>

#include "utils.h"
#include "structs.h"
#include "strings.h"

struct honeypot_t Honeypots[]; // define array for honeypot clients instead of passing structs
char* Paths[];

int connCounter = 0;

void* System(int id)
{

	FILE* filePtr;
	filePtr = fopen("commands.log", "a");

	struct creds_t credentials;
	struct honeypot_t honeypot = Honeypots[id];

	char ConnBuffer[1024];
	read(honeypot.fd, &ConnBuffer, 1024);

	write(honeypot.fd, "dvr login: ", strlen("dvr login: "));
	read(honeypot.fd, &credentials.username, 32);

	Strip(&credentials.username);



	write(honeypot.fd, "password: ", strlen("password: "));
	read(honeypot.fd, &credentials.password, 32);


	Strip(&credentials.password);

	if(credentials.password[0] == '\0' || credentials.username == '\0'){
		printf("[%i] [honeypot] authentication failed (%p)\n", time(NULL), honeypot.fd);
		fprintf(filePtr, "[%i] [honeypot] authentication failed (%p)\n", time(NULL), honeypot.fd);
		write(honeypot.fd, "\n\nLogin failed\r\n", strlen("\n\nLogin failed\r\n"));
		sleep(3);
		shutdown(honeypot.fd, 2);
		pthread_exit(NULL);
	}

	printf("[%i] [honeypot] authentication succeeded [%s:%s] (%p)\n", time(NULL), credentials.username, credentials.password, honeypot.fd);
	fprintf(filePtr, "[%i] [honeypot] authentication succeeded [%s:%s] (%p)\n", time(NULL), credentials.username, credentials.password, honeypot.fd);

	Paths[honeypot.fd] = "/";

	write(honeypot.fd, BusyBox, strlen(BusyBox));



	while(1)
	{
		char Data[1024];

		SendPrompt(honeypot.fd, credentials.username, Paths[honeypot.fd]);
		read(honeypot.fd, Data, 1024);

		Strip(&Data);

		char* Args[1024];

		char* token = strtok(Data, " ");
		size_t i = 0;
		while(token != NULL) 
		{
			Args[i] = token;
			i++;
			token = strtok(NULL, " ");
		}

		if(i != 0)
		{
			if(strstr(Args[0], "clear"))
			{
				write(honeypot.fd, "\033[2J\033[H", strlen("\033[2J\033[H"));
				write(honeypot.fd, BusyBox, strlen(BusyBox));
				printf("[%i] [honeypot] clear called possibly normal user (%p)\n", time(NULL), honeypot.fd);
				fprintf(filePtr, "[%i] [honeypot] clear called possibly normal user (%p)\n", time(NULL), honeypot.fd);
			}
			if(strstr(Args[0], "cd"))
			{
				if(i == 2)
				{
					if(strstr(Args[1], ".."))
					{
						printf("[%i] [honeypot] user changed directory to / (%p)\n", time(NULL), honeypot.fd);
						fprintf(filePtr, "[%i] [honeypot] user changed directory to / (%p)\n", time(NULL), honeypot.fd);
						Paths[honeypot.fd] = "/";
					}
					else
					{
						printf("[%i] [honeypot] user changed directory to %s (%p)\n", time(NULL), Args[1], honeypot.fd);
						fprintf(filePtr, "[%i] [honeypot] user changed directory to %s (%p)\n", time(NULL), Args[1], honeypot.fd);
						Paths[honeypot.fd] = Args[1];
					}
				}
			}
			if(strstr(Args[0], "wget"))
			{
				printf("[%i] [honeypot] wget called with url %s (%p)\n", time(NULL), Args[1], honeypot.fd);
				fprintf(filePtr, "[%i] [honeypot] wget called with url %s (%p)\n", time(NULL), Args[1], honeypot.fd);
				write(honeypot.fd, WGet, strlen(WGet));
			}
			if(strstr(Args[0], "./"))
			{
				printf("[%i] [honeypot] user tried to execute program '%s' (%p)\n", time(NULL), Args[0], honeypot.fd);
				write(honeypot.fd, "\r\n", sizeof("\r\n"));
			}
			if(strstr(Args[0], "ls"))
			{
				printf("[%i] [honeypot] ls was called possibly normal user (%p)\n", time(NULL), honeypot.fd);
				fprintf(filePtr, "[%i] [honeypot] ls was called possibly normal user (%p)\n", time(NULL), honeypot.fd);
				if(strstr(Paths[honeypot.fd], "/"))
				{
					write(honeypot.fd, ListFiles, strlen(ListFiles));
				}
				else
				{
					write(honeypot.fd, "\r\n", strlen("\r\n"));
				}
			}
		}
	}
	fclose(filePtr);
}

void* InitializeHoneypot(int port)
{
	int listen_fd, connection_fd, length; // define sockets
	struct sockaddr_in server_addr, client; // define server address and client address

	printf("[%i] [honeypot] starting service..\n", time(NULL)); // print that honeypot service is starting to backend
	listen_fd = socket(AF_INET, SOCK_STREAM, 0); // Initialize a socket for IPv4 via TCP
	if(listen_fd == -1)  
	{
		printf("[%i] [honeypot] failed to create socket!\n", time(NULL)); // print that it failed to create socket to backend
		exit(0); // shutdown process
	}
	else
		printf("[%i] [honeypot] created socket!\n", time(NULL)); // print that it created a socket successfully to backend

	bzero(&server_addr, sizeof(server_addr)); // zero server_addr's memory context

	server_addr.sin_family = AF_INET; // Sets it to AF_INET / IPv4
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // Select any local ip
	server_addr.sin_port = htons(port); // argv[1] -> port

	if((bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr))) != 0) // bind the server address to the listening socket
	{
		printf("[%i] [honeypot] failed to bind!\n", time(NULL)); // print failed to bind to the backend
		exit(0); // shutdown process
	}
	else
		printf("[%i] [honeypot] successfully binded to 0.0.0.0:%i\n", time(NULL), port); // print that it binded successfully to backend

	if((listen(listen_fd, 10)) != 0) // Start listening
	{
		printf("[%i] [honeypot] failed to start listening!\n", time(NULL)); // prints that is failed to listen to backend
		exit(0); // shutdown process
	}
	else
		printf("[%i] [honeypot] started listening!\n", time(NULL)); // print that i started to listen

	length = sizeof(client); // sets length to size of client address


	while(1){
		connection_fd = accept(listen_fd, (struct sockaddr*)&client, &length); // accept connection
		if(connection_fd == 0)
		{
			printf("[%i] [honeypot] failed to accept connection\n", time(NULL)); // print that honeypot failed to accept connection to backend
		}
		else
		{
			connCounter++;
			printf("[%i] [honeypot] new connection %p\n", time(NULL), connection_fd); // print that it got a new device to backend

			struct honeypot_t honeypot;
			honeypot.fd = connection_fd;
			honeypot.connected_at = time(NULL);

			Honeypots[connCounter] = honeypot;

			pthread_t id; // define pthread_t for the user/malware thread
			pthread_create(&id, 0, &System, connCounter); // create thread for user/malware
		}	
	}
}

int main(int argc, char** argv)
{
	if(argc == 2)
		InitializeHoneypot(atoi(argv[1])); // start on specified port
	else
		InitializeHoneypot(23); // start on default telnet port
}
