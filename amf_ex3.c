#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define GNB_IP "127.0.0.1"  // IP address of bnb
#define GNB_PORT 6000       // Port GNB
#define BUFFER_SIZE 1024

typedef struct {
    int ue_id;
    int message_id;
    int tai;
} ngap_paging_t;

// send NGAP paging
void send_paging_message(int sock) {
    ngap_paging_t paging;
    paging.message_id = 100;
    // paging.ue_id = 123;
    paging.tai = 45204;

    // send 400 rrc pagings
    for (int i = 0; i < 400; i++) {
        paging.ue_id = rand() % 60 + 1;
        if (send(sock, &paging, sizeof(paging), 0) < 0) {
            perror("Send failed");
        } else {
            printf("Sent NGAP Paging to gNB: message_id = %d, ue_id = %d, tai = %d\n",
                paging.message_id, paging.ue_id, paging.tai);
        }
        sleep(1/400);
    }
    
}

int main() {
    int sock;
    struct sockaddr_in gnb_addr;
    char command[BUFFER_SIZE];

    // create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // configure
    memset(&gnb_addr, 0, sizeof(gnb_addr));
    gnb_addr.sin_family = AF_INET;
    gnb_addr.sin_port = htons(GNB_PORT);
    if (inet_pton(AF_INET, GNB_IP, &gnb_addr.sin_addr) <= 0) {
        perror("inet_pton failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // connect
    if (connect(sock, (struct sockaddr*)&gnb_addr, sizeof(gnb_addr)) < 0) {
        perror("Connect failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Connected to gNB at %s:%d\n", GNB_IP, GNB_PORT);

    // "send" command
    while (1) {
        printf("Enter command: ");
        if (fgets(command, sizeof(command), stdin) != NULL) {  
            command[strcspn(command, "\n")] = 0;  

            if (strcmp(command, "send") == 0) {
                send_paging_message(sock); 
            } else {
                printf("Unknown command\n");
            }
        } else {
            perror("Error reading command");
        }
    }

    close(sock);
    return 0;
}