#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <list>
#include <stdexcept>
	

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "Compress.h"
#include "SaveState.h"

int guard(int n, const char * err) { if (n == -1) { perror(err); exit(1); } return n; }

class Connection
{
public:
    int conn_fd;
    int length;
    std::string inbuf;
    bool writing = false;
    SaveObjectMap* omap;
    Connection(int conn_fd_):
        conn_fd(conn_fd_),
        length(-1),
        omap(NULL)
    {
    }
    void process(SaveObjectMap* omap)
    {
        std::string command;
        omap->get_string("command", command);
        if (command == "save")
        {
            std::string steam_username;
            omap->get_string("steam_username", steam_username);
            printf("save: %s %d\n", steam_username.c_str(), omap->get_num("steam_id"));
            std::ofstream outfile (steam_username.c_str());
            omap->get_item("content")->save(outfile);
            outfile.close();
        }
    }

    void recieve()
    {
        static char buf[1024];
        while (true)
        {
            ssize_t num_bytes_received = recv(conn_fd, buf, sizeof(buf), MSG_DONTWAIT);
            if (num_bytes_received  == -1)
            {
                if (errno != EAGAIN && errno != EWOULDBLOCK)
                    close();
                break;
            }
            else if (num_bytes_received  == 0)
            {
                close();
                break;
            }
            inbuf.append(buf, num_bytes_received);
        }
        while (true)
        {
            if (length < 0 && inbuf.length() >= 4)
            {
                length = *(uint32_t*)inbuf.c_str(),
                printf ("length: %d\n", length);
                inbuf.erase(0, 4);
                if (length > 1024*1024)
                {
                    close();
                    break;
                }
            }
            else if (length > 0 && inbuf.length() >= length)
            {
                try
                {
                    printf("fin:%d\n", length);
                    std::string decomp = decompress_string(inbuf);
                    std::istringstream decomp_stream(decomp);
                    SaveObjectMap* omap = SaveObject::load(decomp_stream)->get_map();
                    process(omap);
                    inbuf.erase(0, length);
                    length = -1;
                    delete omap;
                    omap = NULL;
                }
                catch (const std::runtime_error& error)
                {
                    close();
                    break;
                }
            }
            else
                break;
        }
    }
    
    void close()
    {
        if (conn_fd < 0)
            return;
        printf("closed %d\n", conn_fd);
        shutdown(conn_fd, SHUT_WR);
        ::close(conn_fd);
        conn_fd = -1;
    }

};


int main()
{

    struct sockaddr_in myaddr ,clientaddr;
    int sockid;
    sockid=socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK , 0);
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
    
    
    std::list<Connection> conns;
    
    while(true)
    {
        
        {
            fd_set fds;
            struct timespec timeout;
            timeout.tv_sec = 1;
            timeout.tv_nsec = 0;

            FD_ZERO(&fds);
            FD_SET(sockid, &fds);
            
            for (Connection &conn :conns)
            {
                FD_SET(conn.conn_fd, &fds);
            }
            
            select(1024, &fds, NULL, NULL, NULL);
        }
        printf ("start\n");
        
        while (true)
        {
            int conn_fd = accept(sockid,(struct sockaddr *)&clientaddr, &len);
            if (conn_fd == -1)
                break;
            printf("new: %d \n", conn_fd);
            conns.push_back(conn_fd);
        }
        
        std::list<Connection>::iterator it = conns.begin();
        while (it != conns.end())
        {
            Connection& conn = (*it);
            conn.recieve();
            if (conn.conn_fd < 0)
                it = conns.erase(it);
            else
                it++;
        }
    }
                                                                        
//                     std::string decomp = decompress_string(inbuf);
//                     std::istringstream decomp_stream(decomp);
//                     SaveObjectMap* omap = SaveObject::load(decomp_stream)->get_map();
//                     std::string name;
//                     omap->get_string("username", name);
//                     printf("%s %d\n", name.c_str(), omap->get_num("minutes_played"));
//                     {
//                         std::ofstream outfile (name.c_str());
//                         outfile << decomp;
//                     }
    close(sockid);
    return 0;
}
