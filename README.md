# C Chat Prototype

A lightweight client-server chat application implemented in C using standard POSIX socket APIs.

## Features

- **UDP Communication:** Uses Datagram sockets (`SOCK_DGRAM`) for message passing.
- **Client-Server Architecture:** Centralized server handling incoming client connections and message broadcasting.
- **Custom Terminal Handling:** The client intercepts typed characters to provide a clean chat interface by turning off automatic echo.
- **Username Registration:** Users must register with a unique username before sending messages.

## Prerequisites

Before compiling and running the application, ensure you have the following installed:

- **GCC Compiler**
- **POSIX-compliant environment** (Linux, macOS, WSL)

## Usage

- **server:**  
gcc -o server server.c  
./server \<port\>

- **client:**  
gcc -o client client.c  
./client \<server-ip\> \<server-port\>
