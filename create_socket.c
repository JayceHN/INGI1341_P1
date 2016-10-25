#include "create_socket.h"

int create_socket(struct sockaddr_in6 *source_addr, int src_port, struct sockaddr_in6 *dest_addr, int dst_port){
  /******************
  * INIT
  *******************/
  int sockfd = 0;
  int bindfd = 0;
  int connectfd = 0;
  //*****************//

  // create the endpoint
  sockfd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
  // assertion
  if(sockfd < 0){
    perror(strerror(errno));
    return -1;
  }

  //setting the source_addr and binding
  if(source_addr){
    source_addr->sin6_family = AF_INET6;
    source_addr->sin6_port = htons(src_port);
    // assigning a name to a socket
    bindfd = bind(sockfd, (struct sockaddr *) source_addr, sizeof(struct sockaddr_in6));
    // assertion
    if(bindfd < 0){
      perror(strerror(errno));
      return -1;
    }
  }

  //setting the dest_addr and connecting the bound socket to the dest_addr
  if(dest_addr){
    dest_addr->sin6_port = htons(dst_port);
    connectfd = connect(sockfd, (struct sockaddr *) dest_addr, sizeof(struct sockaddr_in6));
    // assertion
    if(connectfd < 0){
      perror(strerror(errno));
      return -1;
    }
  }

  return sockfd;
}

const char * real_address(const char *address, struct sockaddr_in6 *rval){
  // init
  struct addrinfo fam, *tmp;
  memset(&fam, 0, sizeof(fam));

  // setting family for ipv6
  fam.ai_family = AF_INET6;
  // identify an Internet host and a service
  int err = getaddrinfo(address, NULL, &fam, &tmp);
  // assertion
  if(err!=0) return strerror(err);
  // set reval
  rval = memcpy(rval, tmp->ai_addr, sizeof(struct sockaddr_in6));
  return NULL;
}

void read_write_loop(int sfd){

  struct pollfd ufds[2];

  int rv = 0;
  int size = 0;
  char bufRead[1024];
  char bufWrite[1024];
  memset(bufRead, 0, sizeof(bufRead));
  memset(bufWrite, 0, sizeof(bufWrite));

  ufds[0].fd = fileno(stdin);
  ufds[0].events = POLLIN;

  ufds[1].fd = sfd;
  ufds[1].events = POLLIN;

  while(!feof(stdin)){
    rv = poll(ufds, 2, -1);
    if(rv == -1){
      perror("error read_write_loop");
    }

    if(ufds[0].revents & POLLIN){
      if((size = read(fileno(stdin), bufRead, sizeof(bufRead))) < 0){
        fprintf(stderr, "error read_write_loop");
      }
      if(write(sfd, bufRead, size) <0){
        fprintf(stderr, "error read_write_loop");
      }

    }

    if(ufds[1].revents & POLLIN){
      if((size = read(sfd, bufWrite, sizeof(bufWrite))) < 0){
        fprintf(stderr, "error read_write_loop");
      }
      if(write(fileno(stdout), bufWrite, size) < 0){
        fprintf(stderr, "error read_write_loop");
      }
    }
  }

}

int wait_for_client(int sfd){
  char buff[1024];
  socklen_t len;

  struct sockaddr_in6 add;
  memset(&add, 0, sizeof(add));

  if(recvfrom(sfd, buff, sizeof(buff), MSG_PEEK, (struct sockaddr *)&add, &len ) == -1){
    perror(strerror(errno));
    return -1;
  }

  if(connect(sfd, (struct sockaddr*)&add, len) == -1){
    perror(strerror(errno));
    return -1;
  }
  return 0;
}
