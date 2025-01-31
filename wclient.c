#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if(argc != 3){
        printf("enter ./client addr port\n");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in serverAddr;
    int port=atoi(argv[2]);
    printf ("addr: %s\n",argv[1]);
    printf ("port: %i\n",port);

    memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);
    
    serverAddr.sin_addr.s_addr=inet_addr(argv[1]);
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    int clientSocket;
    char buffer[1024];
    char message[16];

    clientSocket = socket(PF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        perror("cannot create socket");
        exit(EXIT_FAILURE);
    }
   
    if (connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof serverAddr) == -1) {
        perror("connect failed");
        close(clientSocket);
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Connection accepted \n");
    }

    sleep(2);

    memset(buffer, 0, sizeof(buffer));
    if (recv(clientSocket, buffer, sizeof(buffer) - 1, 0) < 0) {
        printf("Receive failed.\n");
    } else {
        printf("%s\n", buffer);
    }
    
    for (;;) {
        memset(buffer, 0, sizeof(buffer));
        if (recv(clientSocket, buffer, sizeof(buffer) - 1, 0) < 0) {
            printf("Receive failed.\n");
            break;
        }

        if (strcmp(buffer, "Black wins!") == 0 || strcmp(buffer, "White wins!") == 0) {
            printf("%s\n", buffer);
            break;
            
        }
        printf("%s\n", buffer);

        if (strcmp(buffer, "Your opponent quit game. Ending the game.") == 0 ) {
            break; 
        }
        
        if (strcmp(buffer, "Enter your move (like 5243 or 'exit' to quit the game): ") == 0 ) {
            memset(buffer, 0, sizeof(buffer));
            memset(message, 0, sizeof(message));
            scanf("%s", message);
        }

        if (strcmp(message, "exit") == 0) {
            memset(message, 0, sizeof(message));
            printf("Exiting.\n");
            send(clientSocket, message, strlen(message), 0);
            break;
        }

        send(clientSocket, message, strlen(message), 0);
        memset(message, 0, sizeof(message));
    }

    close(clientSocket);
    return 0;
}