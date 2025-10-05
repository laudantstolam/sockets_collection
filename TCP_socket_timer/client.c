#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#pragma comment(lib, "ws2_32.lib")

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 5678

// [Init]
HINSTANCE hInst;      // handler
HWND hInputA, hInputB, hInputID, hOutput, hSendBtn, hBtnA, hBtnB, hBtnID, hTimerLabel; // input areas
SOCKET sock;          // socket
int step = 0;         // 0=waiting for A, 1=waiting for B, 2=waiting for ID
DWORD startTime = 0;  // Timer start time
int timerActive = 0;  // Timer state

// Forward declaration
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// [Utils] Update timer display
void UpdateTimer(HWND hwnd)
{
    if (timerActive)
    {
        DWORD elapsed = (GetTickCount() - startTime) / 1000;
        char timerText[64];
        snprintf(timerText, sizeof(timerText), "Time elapsed: %lu seconds", elapsed);
        SetWindowText(hTimerLabel, timerText);
    }
    else
    {
        SetWindowText(hTimerLabel, "Timer: Waiting...");
    }
}

// [Utils] Validate string A (5-10 characters)
int validate_string_a(const char *str)
{
    int len = strlen(str);
    return (len >= 5 && len <= 10);
}

// [Utils] Validate string B (even character count)
int validate_string_b(const char *str)
{
    int len = strlen(str);
    return (len > 0 && len % 2 == 0);
}

// [Utils] Add log message to output
void AddLog(const char *msg)
{
    char oldText[8192], newText[16384];
    SYSTEMTIME st;
    GetLocalTime(&st);
    char timestamp[64];

    sprintf(timestamp, "[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);
    GetWindowText(hOutput, oldText, sizeof(oldText));

    snprintf(newText, sizeof(newText), "%s%s%s\r\n", oldText, timestamp, msg);
    SetWindowText(hOutput, newText);
}

// [Utils] Send String A
void SendStringA(HWND hwnd)
{
    char bufferA[1024];
    char logMsg[2048];

    GetWindowText(hInputA, bufferA, sizeof(bufferA));

    // Validate string A
    if (!validate_string_a(bufferA))
    {
        AddLog("ERROR: String A must be 5-10 characters");
        MessageBox(hwnd, "String A must be between 5 and 10 characters!", "Validation Error", MB_OK | MB_ICONERROR);
        return;
    }

    // Send string A
    send(sock, bufferA, strlen(bufferA), 0);
    snprintf(logMsg, sizeof(logMsg), "Sent A: %s", bufferA);
    AddLog(logMsg);

    // Disable button A, enable button B
    EnableWindow(hBtnA, FALSE);
    EnableWindow(hBtnB, TRUE);
    step = 1;
}

// [Utils] Send String B and wait for timeout or ID send
void SendStringB(HWND hwnd)
{
    char bufferB[1024];
    char logMsg[2048];
    char recvbuf[1024];
    int recv_size;
    fd_set readfds;
    struct timeval timeout;

    GetWindowText(hInputB, bufferB, sizeof(bufferB));

    // Validate string B
    if (!validate_string_b(bufferB))
    {
        AddLog("ERROR: String B must have even character count");
        MessageBox(hwnd, "String B must have an even number of characters!", "Validation Error", MB_OK | MB_ICONERROR);
        return;
    }

    // Send string B
    send(sock, bufferB, strlen(bufferB), 0);
    snprintf(logMsg, sizeof(logMsg), "Sent B: %s", bufferB);
    AddLog(logMsg);

    // Disable button B, enable button ID, start timer
    EnableWindow(hBtnB, FALSE);
    EnableWindow(hBtnID, TRUE);
    startTime = GetTickCount();
    timerActive = 1;
    SetTimer(hwnd, 1, 100, NULL); // Update timer every 100ms

    // Also set a 6-second timer to check for server timeout response
    SetTimer(hwnd, 2, 5000, NULL);
    step = 2;
}

// [Utils] Reset to initial state
void ResetToInitial(HWND hwnd)
{
    // Stop all timers
    timerActive = 0;
    KillTimer(hwnd, 1);
    KillTimer(hwnd, 2);

    // Clear all inputs
    SetWindowText(hInputA, "");
    SetWindowText(hInputB, "");
    SetWindowText(hInputID, "");

    // Reset buttons
    EnableWindow(hBtnA, TRUE);
    EnableWindow(hBtnB, FALSE);
    EnableWindow(hBtnID, FALSE);

    // Reset timer display
    UpdateTimer(hwnd);
    step = 0;
}

// [Utils] Send Student ID
void SendStudentID(HWND hwnd)
{
    char bufferID[1024];
    char recvbuf[1024];
    int recv_size;
    char logMsg[2048];

    GetWindowText(hInputID, bufferID, sizeof(bufferID));

    // Validate student ID
    if (strlen(bufferID) == 0)
    {
        AddLog("ERROR: Student ID cannot be empty");
        MessageBox(hwnd, "Please enter your Student ID!", "Validation Error", MB_OK | MB_ICONERROR);
        return;
    }

    // Stop timer
    timerActive = 0;
    KillTimer(hwnd, 1);
    KillTimer(hwnd, 2); // Also kill the timeout check timer
    DWORD elapsed = (GetTickCount() - startTime) / 1000;
    snprintf(logMsg, sizeof(logMsg), "Timer stopped at %lu seconds", elapsed);
    AddLog(logMsg);

    // Send student ID
    send(sock, bufferID, strlen(bufferID), 0);
    snprintf(logMsg, sizeof(logMsg), "Sent ID: %s", bufferID);
    AddLog(logMsg);

    // Receive response from server
    recv_size = recv(sock, recvbuf, sizeof(recvbuf) - 1, 0);
    if (recv_size <= 0)
    {
        AddLog("ERROR: Disconnected from server");
        MessageBox(hwnd, "Disconnected from server.", "Error", MB_OK | MB_ICONERROR);
        PostQuitMessage(0);
        return;
    }
    recvbuf[recv_size] = '\0';

    // Display server response
    snprintf(logMsg, sizeof(logMsg), "Server Response: %s", recvbuf);
    AddLog(logMsg);

    // Reset for next round
    ResetToInitial(hwnd);
}

// [Utils] Check for timeout response from server
void CheckTimeoutResponse(HWND hwnd)
{
    char recvbuf[1024];
    int recv_size;
    char logMsg[2048];
    u_long mode = 1; // Non-blocking mode

    // Set socket to non-blocking to check if data is available
    ioctlsocket(sock, FIONBIO, &mode);

    recv_size = recv(sock, recvbuf, sizeof(recvbuf) - 1, 0);

    // Set socket back to blocking mode
    mode = 0;
    ioctlsocket(sock, FIONBIO, &mode);

    if (recv_size > 0)
    {
        recvbuf[recv_size] = '\0';

        // Check if it's a timeout message
        if (strstr(recvbuf, "Didn't receive student id") != NULL)
        {
            AddLog("Server Timeout: Didn't receive student id in time");
            snprintf(logMsg, sizeof(logMsg), "Server Response: %s", recvbuf);
            AddLog(logMsg);

            // Reset to initial state
            ResetToInitial(hwnd);
        }
    }
}

// Windpws GUI init
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                     LPSTR lpCmdLine, int nCmdShow)
{
    WSADATA wsa;
    struct sockaddr_in server;
    WNDCLASSEX wc;
    HWND hwnd;
    MSG Msg;

    hInst = hInstance;

    // [Init] win socket init
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        MessageBox(NULL, "WSAStartup failed!", "Error", MB_OK);
        return 1;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET)
    {
        MessageBox(NULL, "Socket creation failed!", "Error", MB_OK);
        return 1;
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(SERVER_PORT);
    server.sin_addr.s_addr = inet_addr(SERVER_IP);

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        MessageBox(NULL, "Connect failed!", "Error", MB_OK);
        return 1;
    }

    // Create GUI view
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = 0;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = "ClientWindowClass";
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wc))
    {
        MessageBox(NULL, "Window Registration Failed!", "Error", MB_OK);
        return 1;
    }

    hwnd = CreateWindowEx(0, "ClientWindowClass", "TCP Echo Client",
                          WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                          600, 480, NULL, NULL, hInstance, NULL);
    if (!hwnd)
    {
        MessageBox(NULL, "Window Creation Failed!", "Error", MB_OK);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Keep GUI working
    while (GetMessage(&Msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&Msg);
        DispatchMessage(&Msg);
    }

    closesocket(sock);
    WSACleanup();
    return Msg.wParam;
}

// Window procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    // [Display Init]
    case WM_CREATE:
        // String A input
        CreateWindow("STATIC", "String A (5-10 chars):", WS_VISIBLE | WS_CHILD, 20, 20, 150, 20, hwnd, NULL, hInst, NULL);
        hInputA = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 180, 20, 250, 25, hwnd, NULL, hInst, NULL);
        hBtnA = CreateWindow("BUTTON", "Send A", WS_VISIBLE | WS_CHILD, 440, 20, 80, 25, hwnd, (HMENU)1, hInst, NULL);

        // String B input
        CreateWindow("STATIC", "String B (even chars):", WS_VISIBLE | WS_CHILD, 20, 55, 150, 20, hwnd, NULL, hInst, NULL);
        hInputB = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 180, 55, 250, 25, hwnd, NULL, hInst, NULL);
        hBtnB = CreateWindow("BUTTON", "Send B", WS_VISIBLE | WS_CHILD, 440, 55, 80, 25, hwnd, (HMENU)2, hInst, NULL);
        EnableWindow(hBtnB, FALSE); // Initially disabled

        // Student ID input
        CreateWindow("STATIC", "Student ID:", WS_VISIBLE | WS_CHILD, 20, 90, 150, 20, hwnd, NULL, hInst, NULL);
        hInputID = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 180, 90, 250, 25, hwnd, NULL, hInst, NULL);
        hBtnID = CreateWindow("BUTTON", "Send ID", WS_VISIBLE | WS_CHILD, 440, 90, 80, 25, hwnd, (HMENU)3, hInst, NULL);
        EnableWindow(hBtnID, FALSE); // Initially disabled

        // Timer display
        hTimerLabel = CreateWindow("STATIC", "Timer: Waiting...", WS_VISIBLE | WS_CHILD, 20, 125, 300, 25, hwnd, NULL, hInst, NULL);

        // Output log area
        CreateWindow("STATIC", "Server Response Log:", WS_VISIBLE | WS_CHILD, 20, 160, 200, 20, hwnd, NULL, hInst, NULL);
        hOutput = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY,
                               20, 185, 540, 240, hwnd, NULL, hInst, NULL);
        break;

    // Set button action
    case WM_COMMAND:
        if (LOWORD(wParam) == 1)
            SendStringA(hwnd);
        else if (LOWORD(wParam) == 2)
            SendStringB(hwnd);
        else if (LOWORD(wParam) == 3)
            SendStudentID(hwnd);
        break;

    // Timer update
    case WM_TIMER:
        if (wParam == 1)
            UpdateTimer(hwnd);
        else if (wParam == 2)
            CheckTimeoutResponse(hwnd);
        break;

    // Set close action
    case WM_CLOSE:
        closesocket(sock);
        DestroyWindow(hwnd);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}
