#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define BROADCAST_PORT 5000
#define SERVER_IP "255.255.255.255"
#define BUFFER_SIZE 1024
#define RRC_MESSAGE_ID 100
#define MIB_MESSAGE_ID 1

int ue_id; // UE ID

typedef struct {
    int message_id;
    int sfn;
} mib_t;

typedef struct {
    int message_id;
    unsigned int sfn;
    int pagingRecordList[61];
} rrc_paging_t;

unsigned int sfn = 0; // System Frame Number

// Function for the thread to increment SFN every second
void* sfn_counter(void* arg) {
    while (1) {
        sfn = (sfn + 1) % 1024; // SFN in 5G ranges from 0 to 1023
        printf("Local SFN updated: %u\n", sfn);
        sleep(1);
    }
    return NULL;
}

void* receive_msg(void* arg) {
    int sock;
    struct sockaddr_in server_addr;
    rrc_paging_t rrc_paging;
    socklen_t addr_len = sizeof(server_addr);

    // Create UDP socket
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(BROADCAST_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    // Bind the socket to the server address
    if (bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    while (1) {
         if(recvfrom(sock, &rrc_paging, sizeof(rrc_paging), 0, (struct sockaddr*)&server_addr, &addr_len) < 0){
            perror("recvfrom failed");
            close(sock);
            exit(EXIT_FAILURE);
        }
        else{
            // Update SFN from received MIB message
            if (rrc_paging.message_id == RRC_MESSAGE_ID ) {
                for (int i = 0; i < 61; i++) {
                    if (rrc_paging.pagingRecordList[i] == ue_id) {
                        int received_sfn = rrc_paging.sfn;
                        printf("[SFN = %d]Received RRC Paging: message id = %d , ue_id = %d\n", received_sfn,rrc_paging.message_id,ue_id );
                    }
                    rrc_paging.pagingRecordList[i] = -1; // clear value 
                }
                
            }

            if (rrc_paging.message_id == MIB_MESSAGE_ID) {
                int received_sfn = rrc_paging.sfn;
                printf("Received MIB: message id = %d, updated SFN = %d\n", rrc_paging.message_id, received_sfn);
                sfn = rrc_paging.sfn; 
            }

        }
    }

    close(sock);
    return NULL;
}

int main() {
    ue_id = rand() % 60 + 1;
    printf("----------------- ue_id = %d ----------------\n",ue_id);

    pthread_t sfn_thread, recv_thread;

    // Create a thread to run the SFN counter
    if (pthread_create(&sfn_thread, NULL, sfn_counter, NULL) != 0) {
        perror("Failed to create SFN counter thread");
        exit(EXIT_FAILURE);
    }

    // Create a thread for receiving MIB messages
    if (pthread_create(&recv_thread, NULL, receive_msg, NULL) != 0) {
        perror("Failed to create receive thread");
        exit(EXIT_FAILURE);
    }

    // Wait for threads to finish (they won't, as they run indefinitely)
    pthread_join(sfn_thread, NULL);
    pthread_join(recv_thread, NULL);

    return 0;
}
