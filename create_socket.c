#include "create_socket.h"
#include "packet_interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>

#define BUFFERSIZE 1024

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

  //
  fam.ai_family = AF_UNSPEC;
  fam.ai_socktype = SOCK_DGRAM;
  fam.ai_protocol = IPPROTO_UDP;
  // identify an Internet host and a service
  int err = getaddrinfo(address, NULL, &fam, &tmp);
  // assertion
  if(err!=0) return strerror(err);
  // set reval
  rval = memcpy(rval, tmp->ai_addr, sizeof(struct sockaddr_in6));
  return NULL;
}

void read_write_loop(int sfd, struct sockaddr_in6 *src , struct sockaddr_in6 *dest)
{
  // keeps track of the sequence number
  uint16_t senderSeq = 0;
  uint16_t receiverSeq = 0;
  // buffers where packets are stored
  pkt_t *senderBuffer[5];
  pkt_t *receiverBuffer[5];
  for(int i = 0; i < 5; i++)
  {
    senderBuffer[i] = pkt_new();
    receiverBuffer[i] = pkt_new();
  }

  //keeps track of the size of the sender and receiverBuffer
  uint8_t senderSize = 5;
  uint8_t receiverSize = 5;

  //poll structure
  struct pollfd ufds[2];

  // package and length of addresses
  pkt_t *package;
  socklen_t fromdest = sizeof(struct sockaddr_in6);
  socklen_t fromsrc = sizeof(struct sockaddr_in6);

  // other variables
  int err = 0;
  int rv = 0;
  int size = 0;
  char buffer[BUFFERSIZE];
  char encodeBuffer[BUFFERSIZE];
  size_t len = sizeof(encodeBuffer);

  // initialize buffers
  memset(buffer, 0, sizeof(buffer));
  memset(encodeBuffer, 0, sizeof(encodeBuffer));

  // poll information
  ufds[0].fd = fileno(stdin);
  ufds[0].events = POLLIN;

  ufds[1].fd = sfd;
  ufds[1].events = POLLIN;

  /*
  * A loop where data is read on stdin, converted into a pkt_t structure
  * send over a socket, decoded into a buffer and printed on stdout
  */
  while(!feof(stdin))
  {
    // checks if there is any error relatif to poll
    rv = poll(ufds, 2, -1);
    if(rv == -1)
    {
        perror(strerror(errno));
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
      pkt_set_type(package, PTYPE_DATA);
      //pkt_set_seqnum(package, seqnum);
      pkt_set_timestamp(package, 1); // TODO NOT IMPLEMENTED YET...
      pkt_set_window(package, senderSize);
      pkt_set_payload(package, buffer, size);

      // encode the given package into a new buffer
      pkt_encode(package, encodeBuffer, &len);         //len is updated to the buffer size

      err = sendto(sfd, encodeBuffer, len, 0, (struct sockaddr*)dest, fromdest);
      if(err < 0)
      {
          perror(strerror(errno));
          fprintf(stderr, "error while sending message on the socket \n");
      }

      //increment seqnum, free and reset everything
    //  seqnum++;
      pkt_del(package);
      memset(encodeBuffer, 0, BUFFERSIZE);
      len = BUFFERSIZE;

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

      // DEBUGGIN INFORMATION !
      fprintf(stderr, "SEQNUM : %u\n", pkt_get_seqnum(package));
      fprintf(stderr, "Buffer size : %u\n", pkt_get_window(package));
      // DEBUGGING ENDS HERE

      /*// receiver checks de seqnum
      if(pkt_get_seqnum(package) == receiverSeq)
      {
        // creates a packet with the same seqnum, the window and ACKTYPE
        pkt_t pack = pkt_new();
        pkt_set_type(pack, PTYPE_ACK);
        pkt_set_window(pack, receiverSize);
        pkt_set_seqnum(pack, receiverSeq);

        pkt_encode(pack, encodeBuffer, &len);
        err = sendto(sfd, encodeBuffer, len, 0, (struct sockaddr*)src, &fromsrc);
        if(err < 0)
        {
            perror(strerror(errno));
            fprintf(stderr, "error while sending acknowledgment \n");
        }
      }
        */




      // printing out the payload on stdout
      if(write(fileno(stdout), pkt_get_payload(package), pkt_get_length(package)) < 0)
      {
        fprintf(stderr, "error while writing stdout \n");
      }

      //free package
      pkt_del(package);
      memset(buffer, 0, BUFFERSIZE);

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
