#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <time.h>

#pragma comment(lib, "ws2_32.lib")

// Set socket to non-blocking mode
void set_nonblocking(SOCKET sock) {
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
}

// Check if keyboard input is available (Windows version)
int kbhit() {
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD events;
    INPUT_RECORD buffer;
    PeekConsoleInput(hStdin, &buffer, 1, &events);
    if (events > 0) {
        if (buffer.EventType == KEY_EVENT && buffer.Event.KeyEvent.bKeyDown) {
            return 1;
        }
        ReadConsoleInput(hStdin, &buffer, 1, &events); // Clear event
    }
    return 0;
}

int main(int argc, char *argv[])
{
    WSADATA wsaData;
    SOCKET sock;
    struct sockaddr_in server;
    char buf[1024];
    char num1[100], num2[100];
    char send_msg[256];
    char recv_buf[1024];
    int result;
    time_t start_time;
    int has_second_input;

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

    // Send student ID first
    char studentID[] = "D1480120";
    send(sock, studentID, (int)strlen(studentID), 0);
    printf("Send student ID: %s\n", studentID);
    
    // Wait briefly for server to process student ID
    Sleep(500);

    // Set socket to non-blocking for receiving
    set_nonblocking(sock);

    // Continuous input loop
    while(1) {
        printf("\n=================================\n");
        printf("Enter first number (or 'exit' to quit): ");
        fgets(num1, sizeof(num1), stdin);
        num1[strcspn(num1, "\n")] = 0;

        if(strcmp(num1, "exit") == 0) {
            break;
        }

        // Start 3-second timer
        start_time = time(NULL);
        has_second_input = 0;
        
        printf("Waiting 3 seconds for second number...\n");
        
        // Wait 3 seconds and check for second input
        while(difftime(time(NULL), start_time) < 3.0) {
            if(kbhit()) {
                printf("Enter second number: ");
                fgets(num2, sizeof(num2), stdin);
                num2[strcspn(num2, "\n")] = 0;
                has_second_input = 1;
                break;
            }
            Sleep(100); // Brief delay to avoid CPU spinning
        }

        // Compose message based on whether second input exists
        if(has_second_input) {
            sprintf(send_msg, "%s %s", num1, num2);
            printf("Sending: %s (two numbers)\n", send_msg);
        } else {
            sprintf(send_msg, "%s", num1);
            printf("Sending: %s (one number)\n", send_msg);
        }

        // Send message
        result = send(sock, send_msg, (int)strlen(send_msg), 0);
        if(result == SOCKET_ERROR) {
            printf("Send failed: %ld\n", WSAGetLastError());
            break;
        }

        // Receive result from server
        printf("Waiting for server response...\n");
        int received = 0;
        time_t recv_start = time(NULL);
        
        while(!received && difftime(time(NULL), recv_start) < 5.0) {
            result = recv(sock, recv_buf, sizeof(recv_buf)-1, 0);
            if(result > 0) {
                recv_buf[result] = '\0';
                printf("Server response: %s\n", recv_buf);
                received = 1;
            } else if(result == 0) {
                printf("Server disconnected.\n");
                goto cleanup;
            } else {
                int err = WSAGetLastError();
                if(err != WSAEWOULDBLOCK) {
                    printf("Recv error: %d\n", err);
                    goto cleanup;
                }
            }
            Sleep(100);
        }
        
        if(!received) {
            printf("No response from server (timeout)\n");
        }
    }

cleanup:
    closesocket(sock);
    WSACleanup();
    return 0;
}