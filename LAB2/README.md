### Program Overview

#### Info
- Protocol: TCP
- Language: C
- Server Platform: VirtualBox VM - Linux kali 6.12.25-amd64
- Client Platform: Microsoft Windows11 [10.0.26100.6584]
- IP & Port: Server: 10.0.2.15, Client: 127.0.0.1, Port: `5678`

#### Setup
`tcp_client.c` -> `gcc tcp_client.c -o tcp_client.exe -lws2_32 && ./tcp_client.exe`
`tcp_server.c` -> `gcc tcp_server.c -o server && ./server`

#### Result Preview
![](result.png)

#### Program Flow

```mermaid
sequenceDiagram
    participant Client
    participant Server

    Client->>Server: Connect & send student ID (D1480120)
    Server->>Server: Receive and store student ID
    loop Continuous processing
        Client->>Client: Enter first number
        Client->>Client: Wait 3 seconds for second number
        alt Second number entered
            Client->>Server: Send two numbers
            Server->>Server: Multiply num1 * num2
        else Timeout (no second number)
            Client->>Server: Send one number
            Server->>Server: Multiply num1 * 100
        end
        Server->>Client: Send result
        Client->>Client: Display result
    end
    Client-->>Server: Type 'exit' or disconnect
    Server->>Server: Close connection & exit
```