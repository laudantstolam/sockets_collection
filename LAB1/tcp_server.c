#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc , char *argv[])
{
    struct sockaddr_in server, client;
    int sock, csock;
    char buf[1024];
    socklen_t addressSize;
    int readSize;

    bzero(&server,sizeof(server));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("10.0.2.15");
    server.sin_port = htons(5678);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0) {
        perror("Socket creation failed");
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

    printf("Client connected!\n");

    // 🔹 持續接收資料
    while(1) {
        readSize = recv(csock, buf, sizeof(buf)-1, 0);

        if (readSize > 0) {
            buf[readSize] = '\0';
            printf("Received msg: %s\n", buf);
        }
        else if (readSize == 0) {
            // client 正常斷線
            printf("Client disconnected.\n");
            break;
        }
        else {
            // recv 發生錯誤
            perror("recv failed");
            break;
        }
    }


    close(csock);
    close(sock);
    return 0;
}
