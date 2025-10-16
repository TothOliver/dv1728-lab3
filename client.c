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

ssize_t readMsg(int sockfd, char *buf, size_t bufsize, int seconds);

int main(int argc, char *argv[]){

  /* Do magic */
  if(argc != 3){
    fprintf(stderr, "Usage: %s host:port nickname\n", argv[0]);
    fflush(stderr);
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
      fflush(stderr);
      exit(EXIT_FAILURE);
    }

    size_t host_len = end_bracket - (address_port + 1);
    if(host_len >= sizeof(host)){
      fprintf(stderr, "Error: Host name too long\n");
      fflush(stderr);
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
      fflush(stderr);
      exit(EXIT_FAILURE);
    }
  }
  printf("Host: %s, Port: %s, Nickname: %s\n",host, port, nickname);
  fflush(stdout);

  struct addrinfo hints, *results;
  int sockfd, con;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  int status = getaddrinfo(host, port, &hints, &results);
  if(status != 0 || results == NULL)
  {
    fprintf(stderr, "ERROR: RESOLVE ISSUE");
    fflush(stderr);
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
    fflush(stderr);
    return EXIT_FAILURE;
  }
  if(con == -1){
    fprintf(stderr, "ERROR: CANT CONNECT TO %s\n", host);
    fflush(stderr);
    freeaddrinfo(results);
    return EXIT_FAILURE;
  }

  fd_set reading;
  struct timeval timeout;
  int rc;

  char buf[1000];
  memset(&buf, 0, sizeof(buf));
  ssize_t byte_size;
  byte_size = readMsg(sockfd, buf, sizeof(buf), 2);

  if(byte_size <= 0){
    freeaddrinfo(results);
    close(sockfd);
    fprintf(stderr, "ERROR: read failed!\n");
    fflush(stderr);
    return EXIT_FAILURE;
  }

 if(strcmp(buf, "HELLO 1\n") != 0 && strcmp(buf, "Hello 1.0\n") != 0){
    fprintf(stderr, "ERROR: wrong read: %s\n", buf);
    fflush(stderr);
    freeaddrinfo(results);
    close(sockfd);
    return EXIT_FAILURE;
  }

  memset(&buf, 0, sizeof(buf));
  snprintf(buf, sizeof(buf), "NICK %s\n", nickname);

  ssize_t sent = write(sockfd, buf, strlen(buf));
  if(sent == -1){
    freeaddrinfo(results);
    close(sockfd);
    fprintf(stderr, "ERROR: sendto failed\n");
    fflush(stderr);
    return EXIT_FAILURE;
  }
  printf("Send: %s\n", buf);
  fflush(stdout);

  memset(&buf, 0, sizeof(buf));
  byte_size = readMsg(sockfd, buf, sizeof(buf), 5);

  if(byte_size <= 0){
    freeaddrinfo(results);
    close(sockfd);
    fprintf(stderr, "ERROR: read failed!\n");
    fflush(stderr);
    return EXIT_FAILURE;
  }

  if(strncmp(buf, "OK", 2) != 0){
    freeaddrinfo(results);
    close(sockfd);
    printf("%s\n", buf);
    return EXIT_FAILURE;
  }
  printf("Buf: %s\n", buf);
  fflush(stdout);

  char recvbuf[1024];
  int stdin_closed = 0;
  ssize_t byte_read;
  fd_set readfds;

  struct termios oldt, newt;
  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  setvbuf(stdin, NULL, _IONBF, 0);

  while(1){
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);
    FD_SET(STDIN_FILENO, &readfds);

    int maxfd = (sockfd > STDIN_FILENO) ? sockfd : STDIN_FILENO;
    int rc = select(maxfd + 1, &readfds, NULL, NULL, NULL);
    if(rc < 0){
      perror("select");          
      break;
    }

    if(FD_ISSET(sockfd, &readfds)){
      byte_read = read(sockfd, recvbuf, sizeof(recvbuf) - 1);
      if(byte_read < 0){
        perror("read");
        break;
      }
      else if(byte_read == 0){
        printf("Server closed connection.\n");
        break;
      }

      recvbuf[byte_read] = '\0';

      char* line = strtok(recvbuf, "\n");
      while(line){
        if(strncmp(line, "MSG ", 4) == 0){
          printf("%s\n", line + 4);
          fflush(stdout);
        }
        else if(strncmp(line, "ERROR ", 6) == 0){
          fprintf(stderr, "%s\n", line + 6);
          fflush(stderr);
        }
        else{
          printf("%s\n", line);
          fflush(stdout);
        }
        line = strtok(NULL, "\n");
      }
    }
    if(FD_ISSET(STDIN_FILENO, &readfds) && !stdin_closed){
      char line[254];

      if(fgets(line, sizeof(line), stdin) == NULL){
        stdin_closed = 1;
        continue;
      }

      line[strcspn(line, "\r\n")] = '\0';
      if(line[0] == '\0')
        continue;

      char msg[600];
      snprintf(msg, sizeof(msg), "MSG %s %s\n", nickname, line);
      size_t len = strlen(msg);
      ssize_t sent = send(sockfd, msg, len, MSG_NOSIGNAL);
      if(sent != (ssize_t)len)
        perror("send");

      if(feof(stdin))
        stdin_closed = 1;
    }

  }
  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

  close(sockfd);
  return 0;
}

ssize_t readMsg(int sockfd, char *buf, size_t bufsize, int seconds) {
    fd_set readfds;
    struct timeval timeout;
    int rc;

    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    memset(&timeout, 0, sizeof(timeout));
    timeout.tv_sec = seconds;
    timeout.tv_usec = 0;

    rc = select(sockfd + 1, &readfds, NULL, NULL, &timeout);

    if(rc < 0){
      perror("select");
      return 1;
    } 
    else if(rc == 0){
        fprintf(stderr, "ERROR: Timeout waiting for data\n");
        fflush(stderr);
        return 0;
    }

    ssize_t total = 0;
    ssize_t n;

    while((n = read(sockfd, buf + total, bufsize - 1 - total)) > 0){
      total += n;
      if(memchr(buf, '\n', total) || total >= (ssize_t)(bufsize - 1))
        break;
    }

    if(n < 0){
      perror("read");
      return -1;
    }

    buf[total] = '\0';
    return total;
}
