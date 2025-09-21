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
HWND hInput, hOutput; // input area of winAPI
SOCKET sock;          // socket

// Forward declaration
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// [Utils] Convert letters to uppercase and add spaces between chars
void format_input(const char *input, char *output)
{
    int len = 0;
    for (int i = 0; input[i]; i++)
    {
        if (!isspace(input[i]))
        {
            char c = toupper(input[i]); // Uppercase
            if (len > 0)
                output[len++] = ' '; // Insert space
            output[len++] = c;
        }
    }
    output[len] = '\0';
}

// [Utils] Send input to server and handle response
void SendToServer(HWND hwnd)
{
    // [Init] buffer init
    char buffer[1024];
    char formatted[1024];
    char recvbuf[1024];
    char oldText[4096], newText[8192];
    int recv_size;

    // Get user input from GUI
    GetWindowText(hInput, buffer, sizeof(buffer));
    if (strlen(buffer) == 0)
    {
        MessageBox(hwnd, "Please enter some text.", "Info", MB_OK);
        return;
    }

    format_input(buffer, formatted);

    // Send to server
    send(sock, formatted, strlen(formatted), 0);

    // Receive response
    recv_size = recv(sock, recvbuf, sizeof(recvbuf) - 1, 0);
    if (recv_size <= 0)
    {
        MessageBox(hwnd, "Disconnected from server.", "Error", MB_OK);
        PostQuitMessage(0);
        return;
    }
    recvbuf[recv_size] = '\0';

    // Count letters and numbers
    int letters = 0, numbers = 0;
    char nums[512] = {0}, letters_buf[512] = {0};
    int ncnt = 0, lcnt = 0;

    // Save alphabet and nums seperately
    for (int i = 0; recvbuf[i]; i++)
    {
        if (isspace(recvbuf[i]))
            continue; // Skip spaces
        if (isalpha(recvbuf[i]))
        {
            letters++;
            letters_buf[lcnt++] = recvbuf[i]; // save alphabet to letter buffer
        }
        else if (isdigit(recvbuf[i]))
        {
            numbers++;
            nums[ncnt++] = recvbuf[i]; // save nums to number buffer
        }
    }
    letters_buf[lcnt] = '\0';
    nums[ncnt] = '\0';

    // [Formatting] e.g. [22:58:20] letters: 4 numbers: 2 => [2 4] and [B B K K]
    char display[1024];
    char nums_fmt[512] = "", letters_fmt[512] = "";
    for (int i = 0; i < ncnt; i++)
        snprintf(nums_fmt + strlen(nums_fmt), sizeof(nums_fmt) - strlen(nums_fmt), i == 0 ? "%c" : " %c", nums[i]);
    for (int i = 0; i < lcnt; i++)
        snprintf(letters_fmt + strlen(letters_fmt), sizeof(letters_fmt) - strlen(letters_fmt), i == 0 ? "%c" : " %c", letters_buf[i]);

    snprintf(display, sizeof(display), "letters: %d numbers: %d => [%s] and [%s]", letters, numbers, nums_fmt, letters_fmt);

    // [Display] Update output textbox with timestamp
    SYSTEMTIME st;
    GetLocalTime(&st);
    char timestamp[64]; // add timestamps

    sprintf(timestamp, "[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);
    GetWindowText(hOutput, oldText, sizeof(oldText));

    snprintf(newText, sizeof(newText), "%s%s%s\r\n", oldText, timestamp, display);
    SetWindowText(hOutput, newText);
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

    hwnd = CreateWindowEx(0, "ClientWindowClass", "TCP Client GUI",
                          WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                          400, 300, NULL, NULL, hInstance, NULL);
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
        CreateWindow("STATIC", "Enter text:", WS_VISIBLE | WS_CHILD, 20, 20, 80, 20, hwnd, NULL, hInst, NULL);
        hInput = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER, 100, 20, 200, 25, hwnd, NULL, hInst, NULL);
        CreateWindow("BUTTON", "Send", WS_VISIBLE | WS_CHILD, 310, 20, 60, 25, hwnd, (HMENU)1, hInst, NULL);
        hOutput = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_MULTILINE, 20, 60, 350, 150, hwnd, NULL, hInst, NULL);
        break;

    // Set button action
    case WM_COMMAND:
        if (LOWORD(wParam) == 1)
            SendToServer(hwnd);
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
