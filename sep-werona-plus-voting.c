#include “l7-common.h”

#define SWAP(a, b)                        
do                                    
{                                     
**typeof**(a) __a = (a);          
**typeof**(b) __b = (b);          
**typeof**(*__a) __tmp = *__a;    
*__a = *__b;                      
*__b = __tmp;                     
} while (0)

#define MAX_CLIENTS 10
#define MAX_MSG_LEN 63
#define BACKLOG 3
#define NUM_LAWS 20

typedef struct client{
int client_fd;
char buf[MAX_MSG_LEN+1];
char name[MAX_MSG_LEN+1];
int buf_size;
int32_t vote_count; // stage 7: ile głosów oddał klient
}client_t;

volatile sig_atomic_t do_work = 1;

void sigint_handler(int sig) { do_work = 0; }

void initialize_clients(client_t *clients)
{
for(int i = 0;i < MAX_CLIENTS;i++)
{
clients[i].buf[0] = ‘\0’;
clients[i].buf_size = 0;
clients[i].client_fd = -1;
clients[i].name[0] = ‘\0’;
clients[i].vote_count = 0;
}
}

int find_index(client_t *clients)
{
for(int i = 0;i < MAX_CLIENTS;i++)
{
if(clients[i].client_fd==-1)
return i;
}
return -1;
}

int find_client_index(client_t *clients,int client_fd)
{
for(int i = 0;i < MAX_CLIENTS;i++)
{
if(clients[i].client_fd==client_fd)
return i;
}
return -1;
}

void delete_client(client_t *clients,int client_fd)
{
for(int i = 0;i< MAX_CLIENTS;i++)
{
if(clients[i].client_fd == client_fd)
{
clients[i].buf[0] = ‘\0’;
clients[i].buf_size = 0;
clients[i].client_fd = -1;
clients[i].name[0] = ‘\0’;
clients[i].vote_count = 0;
}
}
close(client_fd);
}

void write_to_everyone(char *message,client_t *clients,int client_fd)
{
for(int i = 0;i<MAX_CLIENTS;i++)
{
if(clients[i].client_fd != -1 && clients[i].client_fd != client_fd)
write(clients[i].client_fd, message, strlen(message));
}
}

// stage 7: parsuje msg formatu “vXX” (bez \n, bo juz usuniete przez while loop)
// zwraca numer prawa [1-20] lub -1 jesli zly format
int parse_vote(char *msg)
{
if(msg[0] != ‘v’) return -1;
if(!isdigit((unsigned char)msg[1]) || !isdigit((unsigned char)msg[2])) return -1;
if(msg[3] != ‘\0’) return -1; // nic wiecej po XX
int num = atoi(msg+1);
if(num < 1 || num > NUM_LAWS) return -1;
return num;
}

// stage 7: wysyla binarnie vote_count jako int32_t w network byte order przed zamknieciem
void finish_job(client_t *clients, int32_t *votes)
{
char goodbye[] = “The agora is closing!\n”;
for(int i = 0;i < MAX_CLIENTS;i++)
{
if(clients[i].client_fd!=-1)
{
write(clients[i].client_fd, goodbye, strlen(goodbye));
// htonl konwertuje z host byte order do network byte order
int32_t count = htonl(clients[i].vote_count);
write(clients[i].client_fd, &count, sizeof(int32_t));
close(clients[i].client_fd);
}
}
printf(“Citizens present: “);
for(int i = 0;i < MAX_CLIENTS;i++)
{
if(clients[i].name[0] != ‘\0’)
printf(”%s “, clients[i].name);
}
printf(”\n”);
// wyniki glosowania
for(int i = 1;i <= NUM_LAWS;i++)
printf(“Law %02d: %d votes\n”, i, votes[i]);
}

void doServer(int tcp_listen_socket)
{
int epoll_descriptor;
if ((epoll_descriptor = epoll_create1(0)) < 0)
ERR(“epoll_create:”);

```
struct epoll_event event, events[MAX_CLIENTS];
event.events = EPOLLIN;
event.data.fd = tcp_listen_socket;
client_t clients[MAX_CLIENTS];
int32_t votes[NUM_LAWS+1]; // indeks 1-20
memset(votes, 0, sizeof(votes));

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
                if(epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, client_fd, &event) == -1)
                    ERR("epoll_ctl");
            }
            else
            {
                ssize_t bytes_read = 0;
                int fd = events[i].data.fd;
                int client_idx = find_client_index(clients, fd);
                if(client_idx==-1) continue;

                if((bytes_read = read(fd, clients[client_idx].buf+clients[client_idx].buf_size, MAX_MSG_LEN-clients[client_idx].buf_size)) == -1)
                    ERR("read");

                if(bytes_read == 0){
                    printf("I lost contact with %s\n", clients[client_idx].name[0] ? clients[client_idx].name : "???");
                    delete_client(clients, fd);
                    continue;
                }
                clients[client_idx].buf_size += bytes_read;
                clients[client_idx].buf[clients[client_idx].buf_size] = '\0';

                char* new_line;
                while((new_line = strchr(clients[client_idx].buf, '\n'))){
                    *new_line = '\0';

                    if(clients[client_idx].name[0] == '\0')
                    {
                        // pierwsza wiadomosc = imie klienta
                        strcpy(clients[client_idx].name, clients[client_idx].buf);
                        char welcome[MAX_MSG_LEN+30];
                        snprintf(welcome, sizeof(welcome), "Welcome to the agora, %s!\n", clients[client_idx].name);
                        write(fd, welcome, strlen(welcome));
                        printf("%s joined the agora\n", clients[client_idx].name);
                    }
                    else
                    {
                        // stage 7: sprawdz czy to glos vXX
                        int law = parse_vote(clients[client_idx].buf);
                        if(law != -1)
                        {
                            // poprawny glos - inkrementuj, rozsij
                            votes[law]++;
                            clients[client_idx].vote_count++;
                            printf("%s voted for law %02d\n", clients[client_idx].name, law);
                            char msg[8];
                            snprintf(msg, sizeof(msg), "v%02d\n", law);
                            write_to_everyone(msg, clients, clients[client_idx].client_fd);
                        }
                        else if(clients[client_idx].buf[0] == 'v')
                        {
                            // zaczyna sie od v ale zly format = rozlacz
                            printf("%s sent invalid vote, disconnected\n", clients[client_idx].name);
                            delete_client(clients, clients[client_idx].client_fd);
                            break; // klient usuniety, przerwij while
                        }
                        else
                        {
                            // zwykla wiadomosc tekstowa - broadcast
                            char message[MAX_MSG_LEN*2+10];
                            snprintf(message, sizeof(message), "[%s]:%s\n", clients[client_idx].name, clients[client_idx].buf);
                            write_to_everyone(message, clients, clients[client_idx].client_fd);
                        }
                    }

                    clients[client_idx].buf_size -= strlen(clients[client_idx].buf)+1;
                    memmove(clients[client_idx].buf, new_line+1, MAX_MSG_LEN-(new_line - clients[client_idx].buf));
                }
            }
        }
    }
    else if(nfds == 0)
    {
        printf("nikogo nie ma\n");
        return;
    }
    else
    {
        if (errno == EINTR) continue;
        ERR("epoll_wait");
    }
}

finish_job(clients, votes);
```

}

int main(int argc, char **argv)
{
if (argc != 2) exit(EXIT_FAILURE);

```
int port = atoi(argv[1]);

if (sethandler(sigint_handler, SIGINT))
    ERR("Seting SIGINT:");
sethandler(SIG_IGN, SIGPIPE);

int tcp_listen_socket;
int new_flags;
tcp_listen_socket = bind_tcp_socket(port, BACKLOG);
new_flags = fcntl(tcp_listen_socket, F_GETFL) | O_NONBLOCK;
fcntl(tcp_listen_socket, F_SETFL, new_flags);
doServer(tcp_listen_socket);
close(tcp_listen_socket);
return EXIT_SUCCESS;
```

}