#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define BROADCAST_PORT 5000
#define BROADCAST_IP "255.255.255.255"
#define BUFFER_SIZE 1024
#define MESSAGE_ID 100

typedef struct {
    int message_id;
    int sfn;
} mib_t;

unsigned int sfn = 0; // System Frame Number

// Function for the thread to increment SFN every second
void* sfn_counter(void* arg) {
    while (1) {
        sfn = (sfn + 1) % 1024; // SFN in 5G ranges from 0 to 1023
        printf("Server GnodeB SFN updated: %u\n", sfn);
        sleep(1);
    }
    return NULL;
}

void* broadcast_mib(void* arg) {
    int sock;
    struct sockaddr_in broadcast_addr;
    int broadcast_permission = 1;
    mib_t mib_message;

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
        // Wait for 8 seconds
        sleep(8);

        // Create MIB message with current SFN
        mib_message.message_id = MESSAGE_ID;
        mib_message.sfn = sfn; // Use the global SFN value

        // Send broadcast message
        if (sendto(sock, &mib_message, sizeof(mib_message), 0, (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
            perror("sendto failed");
            close(sock);
            exit(EXIT_FAILURE);
        }

        printf("Broadcasted: message id = %d, sfn = %d\n", mib_message.message_id, mib_message.sfn);
    }

    close(sock);
    return NULL;
}

int main() {
    pthread_t sfn_thread, broadcast_thread;

    // Create a thread to run the SFN counter
    if (pthread_create(&sfn_thread, NULL, sfn_counter, NULL) != 0) {
        perror("Failed to create SFN counter thread");
        exit(EXIT_FAILURE);
    }

    // Create a thread for broadcasting
    if (pthread_create(&broadcast_thread, NULL, broadcast_mib, NULL) != 0) {
        perror("Failed to create broadcast thread");
        exit(EXIT_FAILURE);
    }

    // Wait for threads to finish (they won't, as they run indefinitely)
    pthread_join(sfn_thread, NULL);
    pthread_join(broadcast_thread, NULL);

    return 0;
}








