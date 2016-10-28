#include "create_socket.h"
#include "packet_interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <time.h>

#define MAXSIZE 1024

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

  const char * real_address(const char *address, struct sockaddr_in6 *rval)
  {
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


int wait_for_client(int sfd)
{
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



void send_loop(int sfd, struct sockaddr_in6 *dest)
{
    uint16_t seqnum = 1;

    pkt_t *senderBuffer[5];
    for(int i = 0; i < 5 ; i++)
    {
      senderBuffer[i] = NULL;
    }

    uint8_t bufferSize = 5;

    int size = 0;
    int rv = 0;

    pkt_t *package1;
    pkt_t *package2;

    socklen_t fromdest = sizeof(struct sockaddr_in6);

    int err = 0;

    char decode[MAXSIZE];
    char encode[MAXSIZE];

    size_t len = MAXSIZE;

    // initialize buffers
    memset(decode, 0, MAXSIZE);
    memset(encode, 0, MAXSIZE);

    uint32_t stamp = 0;
    uint32_t num = 0;

    time_t now;
    struct tm *tm;

    struct pollfd ufds[2];

    ufds[0].fd = fileno(stdin);
    ufds[0].events = POLLIN;
    ufds[1].fd = sfd;
    ufds[1].events = POLLIN;


    while(!feof(stdin))
    {
      rv = poll(ufds, 2, -1);

      if(rv == -1)
      {
          perror(strerror(errno));
      }

        // SENDER is getting data on stdin
        if(ufds[0].revents & POLLIN)
        {
          // if there is something to send and buffer is not full
          if((size = read(fileno(stdin), decode, sizeof(decode))) < 0)
          {
              fprintf(stderr, "Error while reading stdin\n");
          }

          fprintf(stderr, "Reading data from stdin !\n");
          
          if(bufferSize > 0)
          {
            //create a structure with the given information
            package1 = pkt_new();
            pkt_set_type(package1, PTYPE_DATA);
            pkt_set_seqnum(package1, seqnum);
            now = time(0);
            tm = localtime (&now);
            stamp = tm->tm_sec;
            pkt_set_timestamp(package1, stamp);
            pkt_set_window(package1, bufferSize);
            pkt_set_payload(package1, decode, size);
            pkt_encode(package1, encode, &len);
            
            fprintf(stderr, "Data was converted into package, now we send it \n");
            
            err = sendto(sfd, encode, len, 0, (struct sockaddr*)dest, fromdest);
            if(err < 0)
            {
              perror(strerror(errno));
              fprintf(stderr, "error while sending message on the socket \n");
            }

            fprintf(stderr, "pacakge was send with success\n");

            seqnum++;
            memset(encode, 0, MAXSIZE);
            len = MAXSIZE;

            fprintf(stderr, "We put the package into the buffer \n");

            

            for(int i = 0 ; i < 5 ; i++)
            {
              if(senderBuffer[i] == NULL)
              {
                fprintf(stderr, "package put into senderBuffer[%d] !\n", i);
                senderBuffer[i] = package1;
                bufferSize--;
                break;
              }
            }

            fprintf(stderr, "package put into senderBuffer with successs !\n");
          }
        }

        // SENDER is getting acknowledgment on socket
        if(ufds[1].revents & POLLIN)
        {

          fprintf(stderr, "ACKNOWLEGMENT IS RECEIVED !\n");
          //check if we received any acknowledgment from receiver
          if((size = recvfrom(sfd, decode, len, 0, (struct sockaddr *)dest, &fromdest)) < 0)
          {
              fprintf(stderr, "error while receiving on socket \n" );
          }
              package2 = pkt_new();
              pkt_decode(decode, size, package2);

              fprintf(stderr, "ACKNOWLEGMENT TYPE : %u\n", pkt_get_type(package2));
              fprintf(stderr, "ACKNOWLEGMENT SEQNUM : %u\n", pkt_get_seqnum(package2));
              fprintf(stderr, "ACKNOWLEGMENT WINDOW : %u\n", pkt_get_window(package2));

              if(pkt_get_type(package2) == 2)
              {
                  num = pkt_get_seqnum(package2);
                  for(int i = 0; i < 5 ; i++)
                  {
                      if(senderBuffer[i] == NULL)
                      {

                      }
                      else if(pkt_get_seqnum(senderBuffer[i]) <= num)
                      {
                          senderbuffer[i] == NULL;
                          bufferSize++;
                      }
                  }
              }
        }

        if(bufferSize < 5)
        {
          fprintf(stderr, "wee check if we have to resend some packages !\n");
          // check if we have to resend some packages
          for(int i = 0; i < 5 ; i++)
          {

              now = time(NULL);
              tm = localtime (&now);
              stamp = tm->tm_sec;

            //  printf("BufferTime %d : %u | stampTime : %u", i, pkt_get_timestamp(senderBuffer[i]), stamp);
              if(checkTime(pkt_get_timestamp(senderBuffer[i]), stamp) == -1)
              {
                  //send it again
                  pkt_encode(senderBuffer[i], encode, &len);
                  err = sendto(sfd, encode, len, 0, (struct sockaddr*)dest, fromdest);
                  if(err < 0)
                  {
                      perror(strerror(errno));
                    //  fprintf(stderr, "error while sending message on the socket \n");
                  }
              }
          }
        }

    }

}

int checkTime(int time1, int time2)
{
    if(time1 == 61)
    {
      return 0;
    }
    //difference is more than 5sec
    if((time2 - time1) > 5 || (time2 - time1) < -5)
    {
        return -1;
    }
    return 0;
}

void receive_loop(int sfd, struct sockaddr_in6 *dest)
{
    struct pollfd ufds[1];

    ufds[0].fd = sfd;
    ufds[0].events = POLLIN;

    int err = 0;
    int rv = 0;
    int seqnum = 1;
    int size = 0;
    int bufferSize = 5;
    size_t len = MAXSIZE;

    socklen_t fromdest = sizeof(struct sockaddr_in6);
    pkt_t *package;
    pkt_t *ack;

    char decode[MAXSIZE];
    char encode[MAXSIZE];

    memset(decode, 0, MAXSIZE);
    memset(encode, 0, MAXSIZE);

    pkt_t *receiverBuffer[bufferSize];
    for(int i = 0; i < 5 ; i++)
    {
      receiverBuffer[i] = NULL;
    }



    while(1)
    {
      rv = poll(ufds, 1, -1);
      if(rv == -1)
      {
          perror(strerror(errno));
      }

      if(ufds[0].revents & POLLIN)
      {
          fprintf(stderr, "Before receiving messgage \n");
          if((size = recvfrom(sfd, decode, MAXSIZE, 0, (struct sockaddr*)dest, &fromdest)) < 0)
          {
            fprintf(stderr, "error while reading socket !\n");
          }
          package = pkt_new();
          pkt_decode(decode, size, package);
          memset(decode, 0, MAXSIZE);

          fprintf(stderr, "Package decoded ! \n");
          // the received package is the one expected
          if(pkt_get_seqnum(package) == seqnum)
          {
              //printf the package to sdtout
              if(write(fileno(stdout), pkt_get_payload(package), pkt_get_length(package)) < 0)
              {
                fprintf(stderr, "error while writing stdout \n");
              }
              pkt_del(package);

              ack = pkt_new();
              pkt_set_type(ack, PTYPE_ACK);
              pkt_set_window(ack, bufferSize);
              pkt_set_seqnum(ack, seqnum);

              pkt_encode(ack, encode, &len);
              fprintf(stderr, "before sendto ! ! \n");
              err = sendto(sfd, encode, len, 0, (struct sockaddr*)dest, fromdest);
              if(err < 0)
              {
                  perror(strerror(errno));
                  fprintf(stderr, "error while sending acknowledgment \n");
              }
              seqnum++;
              //check if there are valid packages in the buffer
              for(int i = 0; i < 5 ; i++)
              {
                  if(pkt_get_seqnum(receiverBuffer[i]) == seqnum)
                  {
                      if(write(fileno(stdout), pkt_get_payload(receiverBuffer[i]), pkt_get_length(receiverBuffer[i])) < 0 )
                      {
                        printf("error while writing to stdout \n");
                      }
                      pkt_del(receiverBuffer[i]);
                      bufferSize++;
                      seqnum++;
                      i = 0;
                  }
              }
          }

          // the received package is out of order
          for(int i = 0; i < 5; i++)
          {
              if(receiverBuffer[i] == NULL)
              {
                receiverBuffer[i] = package;
                pkt_del(package);
                bufferSize--;
                break;
              }
          }
      }
    }
}
