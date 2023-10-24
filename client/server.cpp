#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <errno.h>

#define PORT 8080

int main(int argc, char const *argv[])
{
    int listen_sd, new_sd, max_sd;
    int i, len, rc, opt = 1;
    int desc_ready, end_server = false; 
    int close_conn; 

    int recv_buf_size = 1024;
    char* recv_buf = (char*) malloc(recv_buf_size);

    struct sockaddr_in addr;
    fd_set master_set, working_set; 
    timeval timeout; 
 
    char *hello = "Hello from server";

    // Creating socket file descriptor
    if ((listen_sd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port 8080
    if (setsockopt(listen_sd, SOL_SOCKET,
                   SO_REUSEADDR | SO_REUSEPORT, &opt,
                   sizeof(opt)))
    {
        perror("setsockopt failed");
        close(listen_sd);
        exit(EXIT_FAILURE);
    }
    // int addrlen = sizeof(address);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    // Forcefully attaching socket to the port 8080
    if (bind(listen_sd, (struct sockaddr *)&addr,
             sizeof(addr)) < 0)
    {
        perror("bind failed");
        close(listen_sd);
        exit(EXIT_FAILURE);
    }
    if (listen(listen_sd, 32) < 0)
    {
        perror("listen failed");
        close(listen_sd);
        exit(EXIT_FAILURE);
    }

    FD_ZERO(&master_set);
    max_sd = listen_sd;
    FD_SET(listen_sd, &master_set);

    timeout.tv_sec = 3 * 60;
    timeout.tv_usec = 0; 

    while (end_server == false) {
        memcpy(&working_set, &master_set, sizeof(master_set));

        printf("Waiting on select()...\n");

        rc = select(max_sd+1, &working_set, NULL, NULL, &timeout);

        if (rc < 0) {
            perror("  select() failed");
            break;
        } 
        if (rc == 0) {
            printf("  select() timed out.  End program.\n");
            break;
        }

        desc_ready = rc; 
        for (i=0; i<=max_sd && desc_ready > 0; i++) {
            if (FD_ISSET(i , &working_set)) {
                desc_ready -= 1;
                if (i == listen_sd) {
                    printf("  Listening socket is readable\n");
                    while (new_sd != -1) {
                        new_sd = accept(listen_sd, NULL, NULL);
                        if (new_sd < 0) {
                            if (errno != EWOULDBLOCK) {
                                perror(" accept() failed");
                                end_server = true;
                            }
                            break;
                        }

                        printf("  New incoming connection - %d\n", new_sd);
                        FD_SET(new_sd, &master_set);
                        if (new_sd > max_sd) 
                            max_sd = new_sd;
                    }
                } else {
                    printf("  Descriptor %d is readable\n", i);
                    close_conn = false;
                    while (true) {
                        rc = recv(i, recv_buf, recv_buf_size, 0);
                        if (rc < 0) {
                            if (errno != EWOULDBLOCK) {
                                perror("  recv failed");
                                close_conn = true;
                            }
                            break;
                        }
                        if (rc == 0) {
                            printf("  Connection closed\n");
                            close_conn = true;
                            break;
                        }

                        len = rc; 
                        printf("  %d bytes recv\n", len);
                        rc = send(i, recv_buf, len, 0);
                        if (rc < 0) {
                            perror("  send failed");
                            close_conn = true; 
                            break; 
                        }
                    }
                    if (close_conn) {
                        close(i);
                        FD_CLR(i, &master_set);
                        if (i == max_sd) {
                            while (FD_ISSET(max_sd, &master_set) == false) {
                                max_sd -= 1;
                            }
                        }
                    }
                } /* End of connection is readable */
            } /* End of if (FD_ISSET(i, &working_set))*/
        } /* End of loop through selectable descriptors */        
    }

    // Clean up 

    for (i=0; i<max_sd; i++) {
        if (FD_ISSET(i, &master_set)) 
            close(i);
    }
}