#include <stdio.h>
#include <stdlib.h>
/* You will to add includes here */
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <termios.h>
#include <time.h>

enum ClientState {STATE_HELLO, STATE_NICK, STATE_CHAT};

typedef struct{
  int fd;
  enum ClientState state;
  char nickname[32];
  int id;
  char ip[64];
  int port;
  time_t connection_start;
}Client;

int main(int argc, char *argv[]){
  
  /* Do more magic */
  if(argc != 2){
    fprintf(stderr, "Usage: %s host:port nickname\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  char* address_port = argv[1];
  char host[256];
  char port[16];

  if(address_port[0] == '['){
    char *end_bracket = strchr(address_port, ']');
    if(!end_bracket){
      fprintf(stderr, "Error: Not a valid IPv6 adress \n");
      exit(EXIT_FAILURE);
    }

    size_t host_len = end_bracket - (address_port + 1);
    if(host_len >= sizeof(host)){
      fprintf(stderr, "Error: Host name too long\n");
      exit(EXIT_FAILURE);
    }

    strncpy(host, address_port + 1, host_len);
    host[host_len] = '\0';

    if(end_bracket[1] != ':'){
      fprintf(stderr, "Error: No host found\n");
      exit(EXIT_FAILURE);
    }
    strncpy(port, end_bracket + 2, sizeof(port) - 1);
    port[sizeof(port) - 1] = '\0';
  }
  else{
    char *colon = strrchr(address_port, ':');
    if(!colon){
      fprintf(stderr, "Error: No host found\n");
      exit(EXIT_FAILURE);
    }

    size_t host_len = colon - address_port;
    if(host_len >= sizeof(host)){
      fprintf(stderr, "Error: Host name too long\n");
      exit(EXIT_FAILURE);
    }

    strncpy(host, address_port, host_len);
    host[host_len] = '\0';

    strncpy(port, colon + 1, sizeof(port) - 1);
    port[sizeof(port) - 1] = '\0';
  }

  struct addrinfo hints, *results;
  int sockfd, bind_status;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  int status = getaddrinfo(host, port, &hints, &results);
  if(status != 0 || results == NULL)
  {
    fprintf(stderr, "ERROR: RESOLVE ISSUE\n");
    return EXIT_FAILURE;
  }

   for(struct addrinfo *p = results; p != NULL; p = p->ai_next){
    sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if(sockfd == -1){
      perror("socket");
      continue;
    }

    int yes = 1;
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1){
      perror("setsockopt");
      close(sockfd);
      sockfd = -1;
      continue;
    }

    if(bind(sockfd, p->ai_addr, p->ai_addrlen) == 0){
      bind_status = 0;
      break;
    }else{
      perror("bind");
      close(sockfd);
      sockfd = -1;
    }
  }
  freeaddrinfo(results);

  if(sockfd == -1){
    fprintf(stderr, "ERROR: CANT CONNECT TO %d\n", sockfd);
    return EXIT_FAILURE;
  }
  if(bind_status == -1){
    freeaddrinfo(results);
    close(sockfd);
    perror("bind");
    fprintf(stderr, "ERROR: CANT BIND to %d\n", sockfd);
    return EXIT_FAILURE;
  }

  int listen_status = listen(sockfd, 10);
  if(listen_status == -1){
    fprintf(stderr, "ERROR: LISTEN FAILED %d\n", sockfd);
    return EXIT_FAILURE;
  }
  
  time_t server_start = time(NULL);
  printf("Server listening...\n");

  Client clients[100];
  for(int i = 0; i < 100; i++){
    clients[i].fd = -1;
    clients[i].state = STATE_HELLO;
  }

  fd_set readfds;
  int rc, maxfd;

  while(1){
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);
    maxfd = sockfd;  

    for (int i = 0; i < 100; i++) {
      if(clients[i].fd != -1){
        FD_SET(clients[i].fd, &readfds);
        if(clients[i].fd > maxfd)
          maxfd = clients[i].fd;
      } 
    }
    rc = select(maxfd+1, &readfds, NULL, NULL, NULL);

    if(rc < 0){
      perror("select");          
      continue;
    }
    if(FD_ISSET(sockfd, &readfds)){
      struct sockaddr_storage client_addr;
      socklen_t addrlen = sizeof(client_addr);

      int newfd = accept(sockfd, (struct sockaddr*)&client_addr, &addrlen);
      if(newfd < 0){
        perror("accept");
        continue;
      }

      char ipstr[64];
      int portnum;

      if(client_addr.ss_family == AF_INET){
          struct sockaddr_in *s = (struct sockaddr_in *)&client_addr;
          inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof(ipstr));
          portnum = ntohs(s->sin_port);
      }else{
          struct sockaddr_in6 *s = (struct sockaddr_in6 *)&client_addr;
          inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof(ipstr));
          portnum = ntohs(s->sin6_port);
      }

      int placed = 0;
      for(int i = 0; i < 100; i++){
        if(clients[i].fd == -1){
          clients[i].fd = newfd;
          clients[i].state = STATE_NICK;
          clients[i].id = i;
          strncpy(clients[i].ip, ipstr, sizeof(clients[i].ip) - 1);
          clients[i].port = portnum;
          clients[i].connection_start = time(NULL);
          write(newfd, "HELLO 1\n", 8);
          placed = 1;
            
          printf("Accepted new client (fd=%d)\n", newfd);
          break;
        }
      }

      if(!placed){
        fprintf(stderr, "Server full, rejecting connection\n");
        close(newfd);
      }
    }
    
    for(int i = 0; i < 100; i++){
      int clientfd = clients[i].fd;
      if(clientfd == -1) 
          continue;

      if(FD_ISSET(clientfd, &readfds)){
        char buf[1000];
        memset(&buf, 0, sizeof(buf));
        ssize_t byte_size = read(clientfd, buf, sizeof(buf) - 1);

        if(byte_size <= 0){
          close(clientfd);
          clients[i].fd = -1;
          printf("Client %d disconnected\n", clientfd);
          continue;
        }

        if(clients[i].state == STATE_NICK){
          if(strncmp(buf, "NICK ", 5) == 0){
            char *nickname = buf + 5;
            nickname[strcspn(nickname, "\r\n")] = '\0';

            if(strlen(nickname) == 0 || strlen(nickname) > 12) {
              write(clientfd, "ERR Nickname too long\n", 22);
              continue;
            }

            for(size_t i = 0; i < strlen(nickname); i++){
              char c = nickname[i];
              if(!isalnum((unsigned char)c) && c != '_'){
                fprintf(stderr, "Error: Nickname contains invalid character\n");
                continue;
              }
            }

            strncpy(clients[i].nickname, buf + 5, sizeof(clients[i].nickname) - 1);
            clients[i].nickname[strcspn(clients[i].nickname, "\r\n")] = '\0';

            write(clientfd, "OK\n", 3);
            clients[i].state = STATE_CHAT;
          }
          else {
            write(clientfd, "ERR Invalid protocol used\n", 4);
          }
        }
        
        else if(clients[i].state == STATE_CHAT){
          if(strncmp(buf, "MSG ", 4) == 0){
            char* msg = buf + 4;
            char output[1000];
            snprintf(output, sizeof(output), "MSG %s %s", clients[i].nickname, msg);
          
            for(int j = 0; j < 100; j++){
              if(clients[j].fd != -1 && clients[j].state == STATE_CHAT){
                ssize_t sent = write(clients[j].fd, output, strlen(output));
                if(sent == -1){
                  perror("write to client");
                }
              }
            }
            printf("%s", output);
          }
          else if(strncmp(buf, "Status\n", 7) == 0){

            int active = 0;
            for(int j = 0; j < 100; j++){
              if(clients[j].fd != -1)
                  active++;
            }
            time_t now = time(NULL);
            long uptime = (long)(now - server_start);
            char statusMsg[1000];
            snprintf(statusMsg, sizeof(statusMsg), "CPSTATUS\n ListenAddress:%s:%s\n Clients %d\nUpTime %ld\n\n", 
              host, port, active, uptime);

            write(clientfd, statusMsg, strlen(statusMsg));
          }
          else if(strncmp(buf, "Clients\n", 8) == 0){
            char reply[2000];
            strcpy(reply, "CPCLIENTS\n");
            time_t now = time(NULL);

            for(int j = 0; j < 100; j++){
              if(clients[j].fd != -1 && clients[j].state == STATE_CHAT){
                long connected_time = (long)(now - clients[j].connection_start);
                char line[1000];
                snprintf(line, sizeof(line),"%d %s %s:%d %ld\n", 
                  clients[j].id, clients[j].nickname[0] ? clients[j].nickname : "(none)", clients[j].ip, clients[j].port, connected_time);
                strncat(reply, line, sizeof(reply) - strlen(reply) - 1);
              }
            }

            strncat(reply, "\n", sizeof(reply) - strlen(reply) - 1);
            write(clientfd, reply, strlen(reply));
          }
          else if(strncmp(buf, "KICK ", 5) == 0){
            char resp[128], target[64], secret[128];
            int parsed = sscanf(buf, "KICK %63s %127s", target, secret);

            if(parsed < 2){
              write(clientfd, "CPKICK: Wrong format\n\n", strlen(resp));
              continue;
            }

            int found = -1;
            for(int j = 0; j < 100; j++){
              if(clients[j].fd != -1 &&
                clients[j].state == STATE_CHAT &&
                strcmp(clients[j].nickname, target) == 0){
                found = j;
                break;
              }
            }

            if(found == -1){
              snprintf(resp, sizeof(resp), "CPKICK: %s not found\n\n", target);
              write(clientfd, resp, strlen(resp));
              continue;
            }

            if(strcmp(secret, "mfo:.ai?fqajdalf832!") != 0){
              write(clientfd, "CPKICK: Wrong secret \n\n", strlen(resp));
              continue;
            }
            
            char kickedMsg[64];
            snprintf(kickedMsg, sizeof(kickedMsg), "KICKED by %s\n", clients[i].nickname);
            write(clients[found].fd, kickedMsg, strlen(kickedMsg));
            close(clients[found].fd);
            clients[found].fd = -1;

            snprintf(resp, sizeof(resp), "CPKICK: %s removed\n\n", target);
            write(clientfd, resp, strlen(resp));

          }
          else{
            char* msg = buf;
            char errorMsg[1000];
            snprintf(errorMsg, sizeof(errorMsg), "ERROR %s", msg);
            write(clientfd, errorMsg, strlen(errorMsg));
          }
        }
      }
    }
  } 
  return 0;
}