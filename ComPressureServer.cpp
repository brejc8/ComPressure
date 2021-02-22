#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "Compress.h"
#include "SaveState.h"

int guard(int n, const char * err) { if (n == -1) { perror(err); exit(1); } return n; }

int main()
{

    struct sockaddr_in myaddr ,clientaddr;
    int sockid,newsockid;
    sockid=socket(AF_INET,SOCK_STREAM,0);
//    memset(&myaddr,'0',sizeof(myaddr));
    myaddr.sin_family=AF_INET;
    myaddr.sin_port=htons(42069);
    myaddr.sin_addr.s_addr=INADDR_ANY;
    if(sockid==-1)
    {
        perror("socket");
    }
    socklen_t len=sizeof(myaddr);
    if(bind(sockid,( struct sockaddr*)&myaddr,len)==-1)
    {
        perror("bind");
    }
    if(listen(sockid,10)==-1)
    {
        perror("listen");
    }
    for(;;)
    {
        int conn_fd = accept(sockid,(struct sockaddr *)&clientaddr, &len);
        
        int pid = fork();
        if (pid == 0)
        {
            pid_t my_pid = getpid();
            printf("%d: forked\n", my_pid);
            char buf[100000];
            std::string inbuf;
            for (;;) {
                ssize_t num_bytes_received = guard(recv(conn_fd, buf, sizeof(buf), 0), "Could not recv on TCP connection");
                printf("got %d\n", num_bytes_received);
                
                if (num_bytes_received == 0)
                {
                    printf("%d: received end-of-connection; closing connection and exiting\n", my_pid);
                    guard(shutdown(conn_fd, SHUT_WR), "Could not shutdown TCP connection");
                    guard(close(conn_fd), "Could not close TCP connection");
                    
                    std::string decomp = decompress_string(inbuf);
                    std::istringstream decomp_stream(decomp);
                    SaveObjectMap* omap = SaveObject::load(decomp_stream)->get_map();
                    std::string name;
                    omap->get_string("username", name);
                    printf("%s %d\n", name.c_str(), omap->get_num("minutes_played"));
                    {
                        std::ofstream outfile (name.c_str());
                        outfile << decomp;
                    }

                    exit(0);
                }
                else
                {
                    inbuf.append(buf, num_bytes_received);
                }

            }
        }
        else
        {
            close(conn_fd);
        }
    }
    close(sockid);
    return 0;
}
