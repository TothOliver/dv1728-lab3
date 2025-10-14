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
#include <termios.h>


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
  printf("We listen :D\n");


  int clients[100];
  for(int i = 0; i < 100; i++)
    clients[i] = -1;

  fd_set readfds;
  int max_fd = sockfd;  
  int rc;

  while(1){
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    rc = select(sockfd+1, &readfds, NULL, NULL, NULL);

    if(rc < 0){
      perror("select");          
      continue;
    }
    if(FD_ISSET(sockfd, &readfds)){
      struct sockaddr_storage client_addr;
      socklen_t addrlen = sizeof(client_addr);
      int new_fd = accept(sockfd, (struct sockaddr*)&client_addr, &addrlen);
      if(new_fd < 0){
        perror("accept");
        continue;
      }

      int placed = 0;
      for(int i = 0; i < 100; i++){
        if(clients[i] == -1){
          clients[i] = new_fd;
          placed = 1;
          break;
        }
      }

      if(!placed){
        fprintf(stderr, "Server full, rejecting connection\n");
        close(new_fd);
      }
      else{
        printf("Accepted new client (fd=%d)\n", new_fd);
      }

    }

  }
}
