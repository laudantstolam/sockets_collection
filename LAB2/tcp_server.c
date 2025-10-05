#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

int main(int argc , char *argv[])
{
    struct sockaddr_in server, client;
    int sock, csock;
    char buf[1024];
    char temp_buf[2048];  // Temporary buffer
    int temp_len = 0;
    socklen_t addressSize;
    int readSize;
    char response[256];
    int first_msg = 1;  // Flag for first message (student ID)

    bzero(&server, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("10.0.2.15");
    server.sin_port = htons(5678);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0) {
        perror("Socket creation failed");
        return -1;
    }

    // Set SO_REUSEADDR to avoid "Address already in use" error
    int opt = 1;
    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Setsockopt failed");
        return -1;
    }

    if(bind(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("Bind failed");
        return -1;
    }

    listen(sock, 5);
    printf("Server listening on port 5678...\n");

    addressSize = sizeof(client);
    csock = accept(sock, (struct sockaddr*)&client, &addressSize);
    if(csock < 0) {
        perror("Accept failed");
        return -1;
    }

    printf("Client connected from %s:%d\n", 
           inet_ntoa(client.sin_addr), 
           ntohs(client.sin_port));

    memset(temp_buf, 0, sizeof(temp_buf));

    // Continuously receive and process data
    while(1) {
        memset(buf, 0, sizeof(buf));
        readSize = recv(csock, buf, sizeof(buf)-1, 0);

        if (readSize > 0) {
            buf[readSize] = '\0';
            
            // First message is student ID, skip processing
            if(first_msg) {
                printf("Received student ID: %s\n", buf);
                first_msg = 0;
                continue;
            }
            
            printf("\n=================================\n");
            printf("Received message: [%s] (length: %d bytes)\n", buf, readSize);

            // Parse message: check if it's one number or two numbers
            int num1 = 0, num2 = 0;
            int count = sscanf(buf, "%d %d", &num1, &num2);

            memset(response, 0, sizeof(response));
            int result_value;
            
            if(count == 2) {
                // Received two numbers: multiply them
                result_value = num1 * num2;
                printf("Processing: %d * %d = %d\n", num1, num2, result_value);
                sprintf(response, "Result: %d * %d = %d", num1, num2, result_value);
            } 
            else if(count == 1) {
                // Received only one number: multiply by 100
                result_value = num1 * 100;
                printf("Processing: %d * 100 = %d\n", num1, result_value);
                sprintf(response, "Result: %d * 100 = %d", num1, result_value);
            }
            else {
                // Cannot parse as number
                printf("Invalid input (not a number)\n");
                sprintf(response, "Error: Invalid input");
            }

            // Send result back to client
            int sendResult = send(csock, response, strlen(response), 0);
            if(sendResult < 0) {
                perror("Send failed");
                break;
            }
            printf("Response sent: [%s]\n", response);
            
            // Brief delay to ensure data transmission completes
            usleep(10000);  // 10ms
        }
        else if (readSize == 0) {
            // Client disconnected normally
            printf("\n=================================\n");
            printf("Client disconnected.\n");
            break;
        }
        else {
            // recv error occurred
            perror("recv failed");
            break;
        }
    }

    close(csock);
    close(sock);
    printf("Server closed.\n");
    return 0;
}