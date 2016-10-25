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
