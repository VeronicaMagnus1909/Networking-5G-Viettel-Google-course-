#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>


//Define for sync
#define BROADCAST_PORT 5000
#define BROADCAST_IP "255.255.255.255"
#define BUFFER_SIZE 1024
#define MESSAGE_ID 100
#define UE_ID 1
#define MESSAGE_ID_MIB 100

// Define for DRX cycle and paging
#define SERVER_PORT 6000
#define DRX_CYCLE 32
#define NPF 16

//queue size
#define MAX_SFN 1024

int queueSize[MAX_SFN]; 


//MIB
typedef struct {
    int message_id;
    int sfn;
} mib_t;

//RRC
typedef struct {
    int message_id;
    int pagingRecordList[MAX_SFN]; // Array of UEs
    int ue_id;
} rrc_paging_t;


//NgAP 
typedef struct {
    int message_id;
    int ue_id;
    int tai;
} ngap_paging_t;

//khai bao ham:
void message_handle(ngap_paging_t* paging);
int enqueueTMSI(int quid, int ueid_temp);

#define QUEUE_SIZE 1024
int enqueueTMSI(int quid, int ueid_temp) {
    // Placeholder logic
    // Return 0 if successful, non-zero if not
    return 0; // Assuming success for demonstration
}

int dequeueTMSI(int sfn, int* pagingRecordList) {
    // Placeholder logic
    // Fill pagingRecordList with UE IDs and return the count
    return 1; // Replace with actual count
}





unsigned int sfn = 0; // System Frame Number

// Function for the thread to increment SFN every second
void* sfn_counter(void* arg) {
    while (1) {
        sfn = (sfn + 1) % 1024; // SFN in 5G ranges from 0 to 1023
        printf("Server SFN updated: %u\n", sfn);
        sleep(1);
    }
    return NULL;
}


//broadcast
void* broadcast_mib(void* arg) {
    int sock;
    struct sockaddr_in broadcast_addr;
    int broadcast_permission = 1;
    mib_t mib_message;
    //struct sockaddr_in rrc_paging;
    rrc_paging_t rrc_paging;

    // Create UDP socket
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Configure socket to allow broadcast
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast_permission, sizeof(broadcast_permission)) < 0) {
        perror("setsockopt failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Configure broadcast address
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(BROADCAST_PORT);
    broadcast_addr.sin_addr.s_addr = inet_addr(BROADCAST_IP);

    while (1) {
    // Check if there are UEs in the queue for the current SFN
    int SFN_temp = sfn;
    //printf("queueSize [SFN_temp]=%d\n sfn luc nay = %d\n", queueSize [SFN_temp],  SFN_temp);
    //start paging
    if (queueSize[SFN_temp] > 0) {
        memset(&rrc_paging, 0, sizeof(rrc_paging));
        int count = dequeueTMSI(SFN_temp, rrc_paging.pagingRecordList);
        if (count > 0) {
            rrc_paging.message_id = MESSAGE_ID;
            rrc_paging.ue_id = UE_ID;
            //rrc_paging.message_id = 
            // Send RRC Paging message via broadcast
            if (sendto(sock, &rrc_paging, sizeof(rrc_paging), 0, (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
                perror("sendto failed");
                close(sock);
                exit(EXIT_FAILURE);
            }

            printf("[SFN = %d] Broadcasted RRC Paging: message_id = %d, UEs count = %d\n", SFN_temp, rrc_paging.message_id, count);
            for (int i = 0; i < count; i++) {
                printf("\t\tUE ID: %d\n", rrc_paging.pagingRecordList[i]);
            }
        }
    }

    // Only broadcast when SFN is divisible by 8
    if (sfn % 8 == 0) {
        // Create MIB message with current SFN
        mib_message.message_id = MESSAGE_ID;
        mib_message.sfn = sfn;

        // Send broadcast message (send the entire struct)
        if (sendto(sock, &mib_message, sizeof(mib_message), 0, (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
            perror("sendto failed");
            close(sock);
            exit(EXIT_FAILURE);
        }

        printf("Broadcasted: message_id = %d, sfn = %d\n", mib_message.message_id, mib_message.sfn);
    }

    sleep(1);
}


    close(sock);
    return NULL;
}

// Message handling from ngap_paging

void message_handle(ngap_paging_t* paging) {
    int ueid_temp = paging->ue_id;
    int drxCycle = DRX_CYCLE;
    int nPF = NPF;
    int SFN_temp = sfn;

    int remainder = (drxCycle / nPF) * ((ueid_temp % 1024) % nPF);

    int quid = 0, offset = 0;
    
    
    while (1) {
        SFN_temp = sfn;
        if (remainder > (SFN_temp % drxCycle)) {
            quid = (SFN_temp + (remainder - (SFN_temp % drxCycle)) + offset * drxCycle) % 1024;
        } else if (remainder == (SFN_temp % drxCycle)) {
            quid = (SFN_temp + (remainder - (SFN_temp % drxCycle)) + (offset + 1) * drxCycle) % 1024;
        } else {
            quid = (SFN_temp + (drxCycle + (remainder - (SFN_temp % drxCycle))) + offset * drxCycle) % 1024;
        }
        

        if (enqueueTMSI(quid, ueid_temp) == 0) {
            printf("[SFN = %u] queue ID = %d ---- TMSI = %d\n", SFN_temp, quid, ueid_temp);
            break;
        } else {
            offset++;
        }
    }
    queueSize[quid] = 1;
    //printf("queueSize[quid] =%d\n",queueSize[quid]);
}

// Function to process connection between TCP and AMF
void* tcp_server(void* arg) {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    int bytes_received;

    // Create socket TCP
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Configure address and port for gNB
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the socket to the address and port
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    // Listen for connections from AMF
    if (listen(server_sock, 5) < 0) {
        perror("listen failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("gNB TCP server is listening on port %d...\n", SERVER_PORT);

    // Accept connections from AMF
    while ((client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_addr_len)) > 0) {
        printf("Connected to AMF\n");

        // Receive data from AMF
        while ((bytes_received = recv(client_sock, buffer, BUFFER_SIZE, 0)) > 0) {
            if (bytes_received < sizeof(ngap_paging_t)) {
                perror("Incomplete message received");
                continue;
            }
            buffer[bytes_received] = '\0';
            ngap_paging_t paging;
            memcpy(&paging, buffer, sizeof(ngap_paging_t));
            message_handle(&paging);
        }

        close(client_sock);
    }

    close(server_sock);
    return NULL;
}



int main() {
    pthread_t sfn_thread, recvNGAP_thread, broadcast_thread;

    // Create a thread to run the SFN counter
    if (pthread_create(&sfn_thread, NULL, sfn_counter, NULL) != 0) {
        perror("Failed to create SFN counter thread");
        exit(EXIT_FAILURE);
    }

    // Create a thread for receiving NgAP messages
    if (pthread_create(&recvNGAP_thread, NULL, tcp_server, NULL) != 0) {
        perror("Failed to create receive thread");
        exit(EXIT_FAILURE);
    }

    // Create a thread for broadcasting
    if (pthread_create(&broadcast_thread, NULL, broadcast_mib, NULL) != 0) {
        perror("Failed to create broadcast thread");
        exit(EXIT_FAILURE);
    }


    // Wait for threads to finish (they won't, as they run indefinitely)
    pthread_join(sfn_thread, NULL);
    pthread_join(recvNGAP_thread, NULL);
    pthread_join(broadcast_thread, NULL);
    return 0;
}


