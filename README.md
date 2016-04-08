# webchat
> A simple (and cheezy) multi-client chat server made purely in C

## Features
* Supports multiple clients at once.
* Nickname feature for each client.
* Works like a chat room (notifies when someone joins / leaves )
* Allows the host (server) to participate.

## Setup
* Clone the repository and `cd` to it.
* Compile and make an executable by `gcc -o server.out server.c`
* Start the server by running the executable. 
The format is `./server.out [Port]` where `[Port]` is the port number you want to listen on.
* Connect to the server using `netcat` or `telnet`

## To-Do / Features to add
A comprehensive To-do list is [here] (https://github.com/mohanagr/webchat/issues)
