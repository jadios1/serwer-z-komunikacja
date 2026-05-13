#include "l7-common.h"


#define SWAP(a, b)                      \
    do                                  \
    {                                   \
        __typeof__(a) __a = (a);        \
        __typeof__(b) __b = (b);        \
        __typeof__(*__a) __tmp = *__a;  \
        *__a = *__b;                    \
        *__b = __tmp;                   \
    } while (0)

#define MAX_CLIENTS 10
#define MAX_MSG_LEN 63
#define BACKLOG 3

typedef struct client{
    int client_fd;
    char buf[MAX_MSG_LEN+1];
    char name[MAX_MSG_LEN+1];
    int buf_size;

}client_t;

volatile sig_atomic_t do_work = 1;


void sigint_handler(int sig) { do_work = 0; }

void initialize_clients(client_t *clients)
{
    for(int i = 0;i < MAX_CLIENTS;i++)
    {
        clients[i].buf[0] = '\0';
        clients[i].buf_size = 0;
        clients[i].client_fd = -1;
        clients[i].name[0] = '\0';
    }
}


int find_index(client_t *clients)
{
    for(int i = 0;i < MAX_CLIENTS;i++)
    {
        if(clients[i].client_fd==-1)
        {
            return i;
        }
    }
    return -1;
}

int find_client_index(client_t *clients,int client_fd)
{
    for(int i = 0;i < MAX_CLIENTS;i++)
    {
        if(clients[i].client_fd==client_fd)
        {
            return i;
        }
    }
    return -1;

}

void delete_client(client_t *clients,int client_fd)
{
    for(int i = 0;i< MAX_CLIENTS;i++)
    {
        if(clients[i].client_fd == client_fd)
        {
            clients[i].buf[0] = '\0';
            clients[i].buf_size = 0;
            clients[i].client_fd = -1;
            clients[i].name[0] = '\0';
        }
    }
    close(client_fd);
}

void write_to_everyone(char *message,client_t *clients,int client_fd)
{
    for(int i = 0;i<MAX_CLIENTS;i++)
    {
        if(clients[i].client_fd != -1 && clients[i].client_fd != client_fd)
        {
            write(clients[i].client_fd, message, strlen(message));
        }
        
    }
}
                  
void finish_job(client_t *clients)
{
    printf("finish_job called!\n");  // ← dodaj
    fflush(stdout);
    char goodbye[] = "agora is closing\n"; 
    for(int i = 0;i < MAX_CLIENTS;i++)
    {
        if(clients[i].client_fd!=-1)
        {
            write(clients[i].client_fd,goodbye,strlen(goodbye));
            close(clients[i].client_fd);
        }


    }


}



void doServer(int tcp_listen_socket)
{
    int epoll_descriptor;
    if ((epoll_descriptor = epoll_create1(0)) < 0)
    {
        ERR("epoll_create:");
    }
    struct epoll_event event, events[MAX_CLIENTS];
    event.events = EPOLLIN;
    
    event.data.fd = tcp_listen_socket;
    client_t clients[MAX_CLIENTS];


    if (epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, tcp_listen_socket, &event) == -1)
    {
        perror("epoll_ctl: listen_sock");
        exit(EXIT_FAILURE);
    }
    int nfds;
    initialize_clients(clients);
    while(do_work)
    {
        if ((nfds = epoll_wait(epoll_descriptor,events,MAX_CLIENTS,40000)) > 0)
        {
            for(int i = 0;i< nfds;i++)
            {

            if(events[i].data.fd==tcp_listen_socket)
            {
                int client_fd = add_new_client(tcp_listen_socket);
                printf("A citizen has arrived at the agora!\n");
                int new_flags = fcntl(client_fd, F_GETFL) | O_NONBLOCK;
                fcntl(client_fd, F_SETFL, new_flags);
                int client_idx = find_index(clients);
                if(client_idx==-1)
                {
                    close(client_fd);
                    close(epoll_descriptor);
                    return;
                }
                clients[client_idx].client_fd = client_fd;
                event.data.fd = client_fd;
                if(epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, client_fd, &event) == -1){
                    ERR("epoll_ctl");
                }
            }
            else
            {
                ssize_t bytes_read = 0;
                int fd = events[i].data.fd;
                int client_idx = find_client_index(clients, fd);
                if(client_idx==-1)
                {
                    continue;
                }
                if((bytes_read = read(fd, clients[client_idx].buf+clients[client_idx].buf_size, MAX_MSG_LEN-clients[client_idx].buf_size)) == -1){
                    ERR("read");
                }
                if(bytes_read == 0){
                    printf("I lost contact with ????");
                    delete_client(clients, fd);
                    continue;
                }
                clients[client_idx].buf_size += bytes_read;


                clients[client_idx].buf[clients[client_idx].buf_size] = '\0';
                char* new_line;
                printf("%s\n", clients[client_idx].buf);

                while((new_line = strchr(clients[client_idx].buf, '\n'))){
                    *new_line = '\0';
                    if(clients[client_idx].name[0] == '\0')
                    {
                    strcpy(clients[client_idx].name, clients[client_idx].buf);
                    char welcome[MAX_MSG_LEN+30];                    
                    snprintf(welcome, sizeof(welcome), "Welcome to the agora, %s!\n", clients[client_idx].name);
                    write(fd, welcome, strlen(welcome));
                    printf("%s joined the agora\n", clients[client_idx].name);
                    }
                    else
                    {
                        char message[MAX_MSG_LEN*2 + 10];                    
                        snprintf(message, sizeof(message), "[%s]:%s\n", clients[client_idx].name,clients[client_idx].buf);
                        write_to_everyone(message,clients,clients[client_idx].client_fd);
                    }
                    
                    clients[client_idx].buf_size -= strlen(clients[client_idx].buf)+1;
                    memmove(clients[client_idx].buf, new_line+1, MAX_MSG_LEN-(new_line - clients[client_idx].buf));
                }
            }
            }

        }
        else if(nfds == 0)
        {
            printf("nikogo nie ma");
            return;
        }
        else
        {
            if (errno == EINTR)
                continue;
            ERR("epoll_wait");
        }
    }
    
    finish_job(clients);
}


int main(int argc, char **argv)
{
    if (argc != 2)
    {
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);

    
    if (sethandler(sigint_handler, SIGINT))
        ERR("Seting SIGINT:");
    
    int tcp_listen_socket;
    int new_flags;

    tcp_listen_socket = bind_tcp_socket(port, BACKLOG);
    new_flags = fcntl(tcp_listen_socket, F_GETFL) | O_NONBLOCK;
    fcntl(tcp_listen_socket, F_SETFL, new_flags);
    doServer(tcp_listen_socket);
    
    close(tcp_listen_socket);


    return EXIT_SUCCESS;
}
