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

#define PORT 5678

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

// [Utils] Increase alphabet
char shift_letter(char c)
{
    // checking ASCII between 65(A)-90(Z)
    if (c >= 'A' && c <= 'Z')
    {
        if (c == 'Z')
        {
            return 'A'; // Z -> A
        }
        else
        {
            return c + 1; // others just +1
        }
    }
    return c;
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
            // receive client data and save to buffer
            ssize_t n = recv(newsockfd, buffer, sizeof(buffer) - 1, 0);
            
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

            buffer[n] = '\0';
            timestamp(ts, sizeof(ts));
            printf(COLOR_INFO "%s Received: %s\n" COLOR_RESET, ts, buffer);

            char letters[512] = {0}, shifted[512] = {0}, nums[512] = {0};
            int lcnt = 0, ncnt = 0;

            // [Processing alphabets]
            for (size_t i = 0; i < strlen(buffer); i++)
            {
                if (isspace(buffer[i]))
                    continue;
                if (isalpha(buffer[i]))
                {
                    // Uppercase alphabet
                    letters[lcnt] = toupper(buffer[i]);
                    // Shift to ASCII+1
                    shifted[lcnt] = shift_letter(letters[lcnt]);
                    lcnt++;
                }
                else if (isdigit(buffer[i]))
                {
                    nums[ncnt++] = buffer[i];
                }
            }
            letters[lcnt] = shifted[lcnt] = '\0';
            nums[ncnt] = '\0';

            // [Formatting]
            char nums_fmt[1024] = {0}, letters_fmt[1024] = {0}, shifted_fmt[1024] = {0};
            // numbers first
            for (int i = 0; i < ncnt; i++)
                snprintf(nums_fmt + strlen(nums_fmt), sizeof(nums_fmt) - strlen(nums_fmt), i == 0 ? "%c" : " %c", nums[i]);
            // origional alphabets
            for (int i = 0; i < lcnt; i++)
                snprintf(letters_fmt + strlen(letters_fmt), sizeof(letters_fmt) - strlen(letters_fmt), i == 0 ? "%c" : " %c", letters[i]);
            // shifted alphabets
            for (int i = 0; i < lcnt; i++)
                snprintf(shifted_fmt + strlen(shifted_fmt), sizeof(shifted_fmt) - strlen(shifted_fmt), i == 0 ? "%c" : " %c", shifted[i]);

            char final_order[2048] = {0};
            if (ncnt > 0)
                strncat(final_order, nums_fmt, sizeof(final_order) - strlen(final_order) - 1);
            if (ncnt > 0 && lcnt > 0)
                strncat(final_order, " ", sizeof(final_order) - strlen(final_order) - 1);
            if (lcnt > 0)
                strncat(final_order, shifted_fmt, sizeof(final_order) - strlen(final_order) - 1);

            timestamp(ts, sizeof(ts));

            //e.g. [09:47:07] Converted: letters=5 numbers=3 [F J I W I] -> [G K J X J] -> [1 1 3 G K J X J]   
            printf(COLOR_INFO "%s Converted: letters=%d numbers=%d [%s] -> [%s] -> [%s]\n" COLOR_RESET,
                   ts, lcnt, ncnt, letters_fmt, shifted_fmt, final_order);

            // return to client e.g. 1 1 3 G K J X J
            send(newsockfd, final_order, strlen(final_order), 0);
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
