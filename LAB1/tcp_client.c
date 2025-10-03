#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

// Link with ws2_32.lib
#pragma comment(lib, "ws2_32.lib")

int main(int argc, char *argv[])
{
    WSADATA wsaData;
    SOCKET sock;
    struct sockaddr_in server;
    char buf[1024];
    int result;

    // Initialize Winsock
    result = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (result != 0) {
        printf("WSAStartup failed: %d\n", result);
        return 1;
    }

    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        printf("Socket creation failed: %ld\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    // Setup server address
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_port = htons(5678);

    // Connect to server
    result = connect(sock, (struct sockaddr*)&server, sizeof(server));
    if (result == SOCKET_ERROR) {
        printf("Connection failed: %ld\n", WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    printf("Connected to server.\n");

    // 先傳送學號
    char studentID[] = "D1480120";
    send(sock, studentID, (int)strlen(studentID), 0);
    printf("Send student ID: %s\n", studentID);

    // 持續從鍵盤輸入文字
    while(1) {
        printf("Enter message: ");
        fgets(buf, sizeof(buf), stdin);
        
        // 移除換行符號
        buf[strcspn(buf, "\n")] = 0;

        if(strcmp(buf, "exit") == 0) {
            break;
        }

        result = send(sock, buf, (int)strlen(buf), 0);
        if(result == SOCKET_ERROR) {
            printf("Send failed: %ld\n", WSAGetLastError());
            break;
        }
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
