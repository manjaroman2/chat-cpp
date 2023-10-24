#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <sys/select.h>
#include <sys/types.h>
#define PORT 8080

bool inputAvailable () {
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    fd_set readfds; 
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    select(STDIN_FILENO+1, &readfds, NULL, NULL, &tv);
    return FD_ISSET(STDIN_FILENO, &readfds);
}


int main(int argc, char const *argv[])
{
    int status, valread, client_fd;
    struct sockaddr_in serv_addr;
    char* hello = "Hello from client";
    int recv_buf_size = 1024;
    char* recv_buf = (char*) malloc(recv_buf_size);
    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        return -1;
    }
    

    fcntl(client_fd, F_SETFL, O_NONBLOCK);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary
    // form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
    {
        printf(
            "\nInvalid address/ Address not supported \n");
        return -1;
    }

    if ((status = connect(client_fd, (struct sockaddr *)&serv_addr,
                          sizeof(serv_addr))) < 0)
    {
        printf("\nConnection Failed \n");
        return -1;
    }
    
    send(client_fd, hello, strlen(hello), 0);
    printf("Hello message sent\n");
    valread = read(client_fd, recv_buf, recv_buf_size);
    printf("%s\n", recv_buf);

    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
    int read_buf_size = 1024;
    char* read_buf = (char*) malloc(read_buf_size);

    int sent;
    
    while (true) {
        if (!inputAvailable()) {
            //valread = read(client_fd, recv_buf, read_buf_size);
            //printf("recv: %s\n", recv_buf);
            std::cout << "no input" << std::endl;
            sleep(500000);
        } else {
            int n = read(STDIN_FILENO, read_buf, read_buf_size);
            printf("read: %s\n", read_buf);
            sent = send(client_fd, read_buf, n, 0);
        }
        
    }
    
    // closing the connected socket
    close(client_fd);
    return 0;
}