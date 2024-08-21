#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#define SERVER_IP "127.0.0.1"  // IP of the gNB (localhost for this example)
#define SERVER_PORT 6000       // Port where the gNB listens
#define BUFFER_SIZE 1024


#define TAI 45204
#define MESSAGE_ID 100

typedef struct {
    int message_id;  // Fixed from message_type
    int ue_id;
    int tai;
} ngap_paging_t;

int send_paging_message(int sock) {
    ngap_paging_t paging;
    paging.message_id = MESSAGE_ID;
    paging.ue_id = 1;
    paging.tai = TAI;

for(int i; i < 10; i++)
{
    if (send(sock, &paging, sizeof(paging), 0) < 0) {
        perror("send failed");
        return -1;
    } else {
        printf("Sent NGAP Paging to gNB: message_id = %d, ue_id = %d, tai = %d\n",
               paging.message_id, paging.ue_id, paging.tai);
    }
    sleep(1/10);
}
    
    return 0;
}


    

int main() {
    int sock;
    struct sockaddr_in gnb_addr;
    char command[BUFFER_SIZE];

    // Create TCP socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Configure gNB address
    memset(&gnb_addr, 0, sizeof(gnb_addr));
    gnb_addr.sin_family = AF_INET;
    gnb_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &gnb_addr.sin_addr) <= 0) {
        perror("inet_pton failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Connect to gNB
    if (connect(sock, (struct sockaddr*)&gnb_addr, sizeof(gnb_addr)) < 0) {
        perror("connect failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Connected to gNB at %s:%d\n", SERVER_IP, SERVER_PORT);

    // Receive commands from user
    while (1) {
        printf("Enter command: ");
        if (fgets(command, sizeof(command), stdin) != NULL) {
            command[strcspn(command, "\n")] = 0; // Remove newline character
            if (strcmp(command, "send") == 0) {
                send_paging_message(sock);
            } else {
                printf("Unknown command\n");
            }
        } else {
            perror("Error reading command");
            close(sock);
            return 0;
        }
    }

    close(sock);
    return 0;
}




