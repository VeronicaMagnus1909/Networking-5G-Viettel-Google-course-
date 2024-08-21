#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define SERVER_PORT 6000
#define BROADCAST_PORT 5000
#define BUFFER_SIZE 1024
#define DRX_CYCLE 32
#define NPF 4
#define RRC_MESSAGE_ID 100
#define MIB_MESSAGE_ID 1

typedef struct {
    int ue_id;
} ngap_paging_t;

typedef struct {
    int message_id;
    unsigned int sfn;
    int pagingRecordList[61];
} rrc_paging_t;

typedef struct {
    int message_id;
    unsigned int sfn;
} mib_message_t;

// Global variables
unsigned int sfn = 0;
int queueSize[1024] = {0};
int pagingQueue[1024][61];
int sock;
struct sockaddr_in broadcast_addr;

// Function prototypes
void* tcp_server(void* arg);
void message_handler(ngap_paging_t* paging_arg);
void* broadcast_rrc_paging(rrc_paging_t* paging) ;
int enqueueTMSI(int qid, int ue_id);
int dequeueTMSI(int qid, int *pagingRecordList);

int main() {
    pthread_t server_thread;

    // Create a thread for the TCP server
    if (pthread_create(&server_thread, NULL, tcp_server, NULL) != 0) {
        perror("pthread_create failed");
        exit(EXIT_FAILURE);
    }

    // Configure broadcast address
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(BROADCAST_PORT);
    broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    // Create socket for broadcasting
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options for broadcasting
    int broadcastEnable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) < 0) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Main loop to handle SFN, Paging, and MIB broadcast
    while (1) {
        // Check if there are UEs in the queue at the current SFN
        if (queueSize[sfn] > 0) {
            rrc_paging_t rrc_paging;
            memset(&rrc_paging, 0, sizeof(rrc_paging));
            int count = dequeueTMSI(sfn, rrc_paging.pagingRecordList);
            if (count > 0) {
                rrc_paging.message_id = RRC_MESSAGE_ID;
                rrc_paging.sfn = sfn;
                // Send RRC Paging message via broadcast
                if (sendto(sock, &rrc_paging, sizeof(rrc_paging), 0, (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
                    perror("sendto failed");
                    exit(EXIT_FAILURE);
                }
                printf("[SFN = %u] Broadcasted RRC Paging: message_id = %d, UEs count = %d\n", sfn, rrc_paging.message_id, count);
                for (int i = 0; i < count; i++) {
                    printf("\t\tUE ID: %d\n", rrc_paging.pagingRecordList[i]);
                }
            }
        }

        // Broadcast MIB message every 8 SFN
        if (sfn % 8 == 0) {
            mib_message_t mib_message;
            mib_message.message_id = MIB_MESSAGE_ID;
            mib_message.sfn = sfn;
            if (sendto(sock, &mib_message, sizeof(mib_message), 0, (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
                perror("sendto failed");
                exit(EXIT_FAILURE);
            }
            printf("Broadcasted: message_id = %d, sfn = %u\n", mib_message.message_id, mib_message.sfn);
        }

        sfn = (sfn + 1) % 1024;
        sleep(1);
    }

    pthread_join(server_thread, NULL);
    close(sock);
    return 0;
}

void* tcp_server(void* arg) {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    int bytes_received;

    // Create TCP socket
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Configure address and port for the server
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Set socket options
    int opt = 1;
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Bind socket to address and port
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_sock, 5) < 0) {
        perror("listen failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("TCP server is listening on port %d...\n", SERVER_PORT);

    // Accept connections from clients
    while ((client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_addr_len)) > 0) {
        printf("Connected to client\n");

        // Receive data from client
        while ((bytes_received = recv(client_sock, buffer, BUFFER_SIZE, 0)) > 0) {
            buffer[bytes_received] = '\0';
            // Process received data
            ngap_paging_t paging;
            memcpy(&paging, buffer, sizeof(ngap_paging_t));
            // pthread_t handle_msg;
            // pthread_create(&handle_msg, NULL, message_handler,(void *) &paging);
            message_handler(&paging);
        }

        close(client_sock);
    }

    close(server_sock);
    return NULL;
}

void message_handler(ngap_paging_t* paging_arg) {
    ngap_paging_t paging = *paging_arg;
    int ue_id = paging.ue_id;
    int drxCycle = DRX_CYCLE;
    int nPF = NPF;
    int SFN_temp = sfn;
    int remainder = (drxCycle / nPF) * ((ue_id % 1024) % nPF);
    int qid = 0, offset = 0;

    while (1) {
        SFN_temp = sfn;
        // Calculate SFN offset to SFN target
        if (remainder > (SFN_temp % drxCycle)) {
            qid = (SFN_temp + (remainder - (SFN_temp % drxCycle)) + offset * drxCycle) % 1024;
        } else if (remainder == (SFN_temp % drxCycle)) {
            qid = (SFN_temp + (remainder - (SFN_temp % drxCycle)) + (offset + 1) * drxCycle) % 1024;
        } else {
            qid = (SFN_temp + (drxCycle + (remainder - (SFN_temp % drxCycle))) + offset * drxCycle) % 1024;
        }

        if (enqueueTMSI(qid, ue_id) == 0) {
            printf("[SFN = %u] queue ID = %d, TMSI = %d\n", SFN_temp, qid, ue_id);
            break;
        } else {
            offset++;
        }
    }
}

int enqueueTMSI(int qid, int ue_id) {
    if (queueSize[qid] < 10) {
        pagingQueue[qid][queueSize[qid]++] = ue_id;
        return 0;
    }
    return -1;
}

int dequeueTMSI(int qid, int *pagingRecordList) {
    int count = queueSize[qid];
    for (int i = 0; i < count; i++) {
        pagingRecordList[i] = pagingQueue[qid][i];
    }
    queueSize[qid] = 0; // Clear the queue after dequeuing
    return count;
}
