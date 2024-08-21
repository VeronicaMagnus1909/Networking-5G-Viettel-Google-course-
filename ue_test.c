#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define SERVER_PORT 5000
#define SERVER_IP "255.255.255.255"
#define BUFFER_SIZE 1024

#define UE_ID 1
#define MESSAGE_ID 100

typedef struct {
    int message_id;
    int sfn;
} mib_t;

#define MAX_SFN 1024
//RRC
typedef struct {
    int message_id;
    int pagingRecordList[MAX_SFN]; // Array of UEs
    int ue_id;
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

void* receive_mib(void* arg) {
    int sock;
    struct sockaddr_in server_addr;
    mib_t mib_message;
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
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    // Bind the socket to the server address
    if (bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    while (1) {

        // Receive MIB message from server
        if (recvfrom(sock, &mib_message, sizeof(mib_message), 0, (struct sockaddr*)&server_addr, &addr_len) < 0) {
            perror("recvfrom failed");
            close(sock);
            exit(EXIT_FAILURE);
        }else{
            // Update SFN from received MIB message
            if (mib_message.message_id == 100) {
                int received_sfn = mib_message.sfn;
                printf("Received MIB: message id = %d, updated SFN = %d\n", mib_message.message_id, received_sfn);
            }
            sfn = mib_message.sfn; //synchronize sfn
        }

        //receive rrc paging 
        if(recvfrom(sock, &rrc_paging, sizeof(rrc_paging), 0, (struct sockaddr*)&server_addr, &addr_len) < 0){
            perror("recv rrc paging failed");
            close(sock);
            exit(EXIT_FAILURE);
        }
        else{
            
            if (rrc_paging.message_id == MESSAGE_ID && rrc_paging.ue_id == UE_ID  ) {
                printf("Receive RRC success\n");

                
            }
        }
        

       

        
    }

    close(sock);
    return NULL;
}

int main() {
    pthread_t sfn_thread, recv_thread;

    // Create a thread to run the SFN counter
    if (pthread_create(&sfn_thread, NULL, sfn_counter, NULL) != 0) {
        perror("Failed to create SFN counter thread");
        exit(EXIT_FAILURE);
    }

    // Create a thread for receiving MIB messages
    if (pthread_create(&recv_thread, NULL, receive_mib, NULL) != 0) {
        perror("Failed to create receive thread");
        exit(EXIT_FAILURE);
    }

    // Wait for threads to finish (they won't, as they run indefinitely)
    pthread_join(sfn_thread, NULL);
    pthread_join(recv_thread, NULL);

    return 0;
}