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

int main(int argc, char *argv[]){

  /* Do magic */
  if(argc != 3){
    fprintf(stderr, "Usage: %s host:port nickname\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  char* address_port = argv[1];
  char* nickname = argv[2];
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
  
  if(strlen(nickname) > 12){
    fprintf(stderr, "Error: Nickname to long\n");
    exit(EXIT_FAILURE);
  }

  for(size_t i = 0; i < strlen(nickname); i++){
    char c = nickname[i];
    if(!isalnum((unsigned char)c) && c != '_'){
      fprintf(stderr, "Error: Nickname contains invalid character\n");
      exit(EXIT_FAILURE);
    }
  }

  printf("Host: %s, Port: %s, Nickname: %s\n",host, port, nickname);

  struct addrinfo hints, *results;
  int sockfd, con;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  int status = getaddrinfo(host, port, &hints, &results);
  if(status != 0 || results == NULL)
  {
    fprintf(stderr, "ERROR: RESOLVE ISSUE");
    return EXIT_FAILURE;
  }

  for(struct addrinfo *p = results; p != NULL; p = p->ai_next){
    sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if(sockfd == -1){
      continue;
    }

    con = connect(sockfd, p->ai_addr, p->ai_addrlen);
    if(con == -1){
      close(sockfd);
      sockfd = -1;
      continue;
    }
    break;
  }

  if(sockfd == -1){
    fprintf(stderr, "ERROR: socket failed\n");
    return EXIT_FAILURE;
  }
  if(con == -1){
    fprintf(stderr, "ERROR: CANT CONNECT TO %s\n", host);
    freeaddrinfo(results);
    return EXIT_FAILURE;
  }

  printf("connected!\n");
}
