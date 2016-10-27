#include "create_socket.h"
#include "packet_interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>

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

void read_write_loop(int sfd, struct sockaddr_in6 *src , struct sockaddr_in6 *dest){

  struct pollfd ufds[2];
  pkt_t *package;
  int err = 0;
  socklen_t fromdest = sizeof(struct sockaddr_in6);
  socklen_t fromsrc = sizeof(struct sockaddr_in6);

  int rv = 0;
  int size = 0;
  char buffer[1024];
  char encodeBuffer[1024];
  size_t len = sizeof(encodeBuffer);

  memset(buffer, 0, sizeof(buffer));
  memset(encodeBuffer, 0, sizeof(encodeBuffer));

  ufds[0].fd = fileno(stdin);
  ufds[0].events = POLLIN;

  ufds[1].fd = sfd;
  ufds[1].events = POLLIN;

  while(!feof(stdin))
  {
    rv = poll(ufds, 2, -1);
    if(rv == -1){
      perror("error read_write_loop");
    }

    // SENDER is reading stdin and sending it on the socket
    if(ufds[0].revents & POLLIN)
    {
      //reads from stdin
      if((size = read(fileno(stdin), buffer, sizeof(buffer))) < 0)
      {
        // error while reading stdin
        fprintf(stderr, "error while reading stdin \n");
      }

      //create a structure with the given information
      package = pkt_new();
      pkt_set_type(package, 1);
      pkt_set_timestamp(package, 1);
      pkt_set_window(package, 1);
      pkt_set_payload(package, buffer, size);

      // encode the given package into a new buffer
      pkt_encode(package, encodeBuffer, &len);         //len is updated to the buffer size

      err = sendto(sfd, encodeBuffer, len, 0, (struct sockaddr*)dest, fromdest);
      if(err < 0)
      {
          perror(strerror(errno));
          fprintf(stderr, "error while sending message on the socket \n");
      }

    }

    // RECEIVER  is reading the socket and printing the result on stdout
    if(ufds[1].revents & POLLIN)
    {
      // receiver is reading the socket
      if((size = recvfrom(sfd, buffer, sizeof(buffer), 0, (struct sockaddr*)src, &fromsrc)) < 0)
      {
        fprintf(stderr, "error while reading on socket \n");
      }

      //decoding the packet into a structure
      package = pkt_new();
      pkt_decode(buffer, size, package);

      // printing out the payload on stdout
      if(write(fileno(stdout), pkt_get_payload(package), pkt_get_length(package)) < 0)
      {
        fprintf(stderr, "error while writing stdout \n");
      }
    }
  }// end while loop
}//end function

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
