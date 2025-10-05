// socket related
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <sys/select.h>

#define PORT 5678
#define TIMEOUT_SECONDS 5

// [Init] Set ANSI color
#define COLOR_RESET "\033[0m"
#define COLOR_INFO "\033[32m"
#define COLOR_ERROR "\033[31m"
#define COLOR_WARN "\033[33m"

// [Utils] Get timestamps
void timestamp(char *buf, size_t size)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    snprintf(buf, size, "[%02d:%02d:%02d]", t->tm_hour, t->tm_min, t->tm_sec);
}

// server thread
void *server_thread(void *arg)
{
    // [Init] TCP socket & msg buffer
    int sockfd, newsockfd;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;
    char buffer[1024];

    // Ipv4 TCP socket = AF_INET
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        return NULL;
    }

    // Set server address and port
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);

    // reusing port --> peevent server stuck form TCP time wait
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // bind socket to port(5678)
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("bind");
        close(sockfd);
        return NULL;
    }

    if (listen(sockfd, 5) < 0)
    {
        perror("listen");
        close(sockfd);
        return NULL;
    }

    printf(COLOR_INFO "[INFO] Server started, waiting for clients...\n" COLOR_RESET);

    // Main loop
    while (1)
    {
        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0)
        {
            perror("accept");
            continue;
        }

        char ts[32];
        timestamp(ts, sizeof(ts));
        printf(COLOR_INFO "%s Client connected: %s:%d\n" COLOR_RESET,
               ts, inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

        while (1)
        {
            char stringA[1024] = {0};
            char stringB[1024] = {0};
            char studentID[1024] = {0};
            char response[2048] = {0};
            fd_set readfds;
            struct timeval timeout;
            ssize_t n;

            // Step 1: Receive string A
            n = recv(newsockfd, stringA, sizeof(stringA) - 1, 0);
            if (n <= 0)
            {
                timestamp(ts, sizeof(ts));
                if (n == 0)
                    printf(COLOR_WARN "%s Client has closed the connection.\n" COLOR_RESET, ts);
                else
                    perror("recv");
                close(newsockfd);
                close(sockfd);
                exit(0);
            }
            stringA[n] = '\0';
            timestamp(ts, sizeof(ts));
            printf(COLOR_INFO "%s Received String A: %s\n" COLOR_RESET, ts, stringA);

            // Step 2: Receive string B
            n = recv(newsockfd, stringB, sizeof(stringB) - 1, 0);
            if (n <= 0)
            {
                timestamp(ts, sizeof(ts));
                if (n == 0)
                    printf(COLOR_WARN "%s Client has closed the connection.\n" COLOR_RESET, ts);
                else
                    perror("recv");
                close(newsockfd);
                close(sockfd);
                exit(0);
            }
            stringB[n] = '\0';
            timestamp(ts, sizeof(ts));
            printf(COLOR_INFO "%s Received String B: %s\n" COLOR_RESET, ts, stringB);

            // Step 3: Wait for student ID with 5-second timeout
            FD_ZERO(&readfds);
            FD_SET(newsockfd, &readfds);
            timeout.tv_sec = TIMEOUT_SECONDS;
            timeout.tv_usec = 0;

            timestamp(ts, sizeof(ts));
            printf(COLOR_INFO "%s Waiting for Student ID (timeout: %d seconds)...\n" COLOR_RESET,
                   ts, TIMEOUT_SECONDS);

            // Record start time for timer display
            time_t start_wait = time(NULL);
            int select_result = select(newsockfd + 1, &readfds, NULL, NULL, &timeout);
            time_t end_wait = time(NULL);
            int elapsed_seconds = (int)difftime(end_wait, start_wait);

            if (select_result > 0)
            {
                // Data is available, receive student ID
                n = recv(newsockfd, studentID, sizeof(studentID) - 1, 0);
                if (n <= 0)
                {
                    timestamp(ts, sizeof(ts));
                    if (n == 0)
                        printf(COLOR_WARN "%s Client has closed the connection.\n" COLOR_RESET, ts);
                    else
                        perror("recv");
                    close(newsockfd);
                    close(sockfd);
                    exit(0);
                }
                studentID[n] = '\0';
                timestamp(ts, sizeof(ts));
                printf(COLOR_INFO "%s Received Student ID: %s (Timer: %d seconds)\n" COLOR_RESET,
                       ts, studentID, elapsed_seconds);

                // Form response: (A)+(B)+": "+ student id
                snprintf(response, sizeof(response), "%s%s: [ %s ]", stringA, stringB, studentID);
                timestamp(ts, sizeof(ts));
                printf(COLOR_INFO "%s Sending response: %s\n" COLOR_RESET, ts, response);
            }
            else if (select_result == 0)
            {
                // Timeout occurred
                strcpy(response, "Didn't receive student id");
                timestamp(ts, sizeof(ts));
                printf(COLOR_WARN "%s Timeout! %s (Timer: %d seconds)\n" COLOR_RESET,
                       ts, response, elapsed_seconds);
            }
            else
            {
                // Error occurred
                perror("select");
                close(newsockfd);
                close(sockfd);
                exit(1);
            }

            // Send response back to client
            send(newsockfd, response, strlen(response), 0);
        }
    }

    close(sockfd);
    return NULL;
}

int main()
{
    pthread_t tid;
    pthread_create(&tid, NULL, server_thread, NULL);
    pthread_join(tid, NULL); // wait for server thread
    return 0;
}
