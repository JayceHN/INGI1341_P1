#include "transport_interface.h"
#include "packet_interface.h"

int real_address(const char *address, struct sockaddr_in6 *rval)
{
	struct addrinfo hints, *res;
	int val = 0;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET6; // ipv6
	hints.ai_socktype = SOCK_DGRAM; // datagramm socket
	hints.ai_protocol = IPPROTO_UDP; // use udp

	if((val = getaddrinfo(address, NULL, &hints, &res)) != 0)
	{
		// failed to get a valid address
		return -1;
	}

	rval = memcpy(rval, res->ai_addr, sizeof(struct sockaddr_in6));
	freeaddrinfo(res);
	return 0;
}


int create_socket(struct sockaddr_in6 *src, int src_port, struct sockaddr_in6 *dest, int dst_port)
{
	int sockfd = 0;
	int bindfd = 0;
	int connectfd = 0;

	//create the socket
	sockfd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);

	// failed to create socket
	if(sockfd < 0)
	{
		perror(strerror(errno));
		return -1;
	}

	// setting src address and binding it to the socket
	if(src != NULL)
	{
		//adding port to src
    	src->sin6_port = htons(src_port);

		// bind source addr to the socket
		bindfd = bind(sockfd, (struct sockaddr *) src, sizeof(struct sockaddr_in6));

		//failed to bind src to the socket
		if(bindfd < 0)
    	{
      		perror(strerror(errno));
      		return -1;
    	}
	}

	//setting dest address and connecting socket
  	if(dest != NULL)
	{
		// adding port do dest
    	dest->sin6_port = htons(dst_port);
    	connectfd = connect(sockfd, (struct sockaddr *) dest, sizeof(struct sockaddr_in6));

		// connect failed
    	if(connectfd < 0)
		{
      		perror(strerror(errno));
      		return -1;
    	}
  	}
	// SUCCESS
	return sockfd;
}

int wait_for_client(int sfd)
{
	char buff[MAX_PACKET_SIZE];
  	socklen_t len;

  	struct sockaddr_in6 add;
  	memset(&add, 0, sizeof(add));

	if(recvfrom(sfd, buff, MAX_PACKET_SIZE, MSG_PEEK, (struct sockaddr *)&add, &len ) == -1)
  	{
    	perror(strerror(errno));
    	return -1;
  	}

  	if(connect(sfd, (struct sockaddr*)&add, len) == -1)
	{
    	perror(strerror(errno));
    	return -1;
  	}

	//SUCCESS
	return 0;
}

uint8_t incSeqNum(uint8_t seqnum)
{
	if(seqnum == 255)
	{
		return 0;
	}
	else
	{
		return (seqnum+1);
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

void sender_loop(int sfd, struct sockaddr_in6 *dest)
{
	// variables
	int size = 0;
	int i;
	char inPut[MAX_PACKET_SIZE];
	char coding[MAX_PACKET_SIZE];
	pkt_t *packet;
	size_t len = MAX_PACKET_SIZE;
	uint8_t num = 0;
	pkt_status_code code;
	socklen_t size_in6 = sizeof(struct sockaddr_in6);


	//variables for timestamp
	time_t now;
    struct tm *tm;
	uint32_t stamp = 0;


	//Sequence number starts with 0
	uint16_t seqNum = 0;

	//Buffer of maximum size
	pkt_t *senderBuffer[MAX_WINDOW_SIZE];

	for(i = 0; i < MAX_WINDOW_SIZE ; i++)
	{
		senderBuffer[i] = NULL;
	}

	// initialize bufferSize
	uint8_t senderBufferSize = MAX_WINDOW_SIZE;
	uint8_t receiverBufferSize = 1; // we assume that there is only one free space at the beginning

	//we read on both : stdin and socket
	struct pollfd ufds[2];
	ufds[0].fd = fileno(stdin);
    ufds[0].events = POLLIN;
    ufds[1].fd = sfd;
    ufds[1].events = POLLIN;
	int rv = 0;

	//while we read something on stdin we continue
	while(!feof(stdin))
	{
		//initializing poll to check if something was received (stdin or socket)
		rv = poll(ufds, 2, -1);
		if(rv == -1)
      	{
          	perror(strerror(errno));
      	}

		//something was received on stdin !
		if(ufds[0].revents & POLLIN)
        {
			// we read what was written on stdin and place it in inPut
			// size contains de number of bytes read
			size = read(fileno(stdin), inPut, MAX_PACKET_SIZE);

			// check if there was no error while reading
			if(size < 0)
			{
				fprintf(stderr, "Error while reading data on stdin ... \n");
				break;
			}

			// We have space in our Buffer and can place it in it
			if(senderBufferSize > 0)
			{
				packet = pkt_new();
				pkt_set_type(packet, PTYPE_DATA); // we send data
				pkt_set_seqnum(packet, seqNum); // set the right seqNum
				seqNum = incSeqNum(seqNum); //increment seqNum

				// check the time in seconds to set timestamp
				now = time(NULL);
            	tm = localtime (&now);
            	stamp = tm->tm_sec;
				pkt_set_timestamp(packet, stamp);

				// Set window decrease bufferSize beforehand
				senderBufferSize--;
				pkt_set_window(packet, senderBufferSize);

				pkt_set_payload(packet, inPut, size);

				// encode and len are going to contain the data to send
				// and the respectif length
				pkt_encode(packet, coding, &len);

				//only send it if receiver has space otherwise place it in the buffer
				if(receiverBufferSize > 0)
				{
					// we send the encoded data on the given socket to the given destination
					// dont need to check if successful because if not we are never going to get
					// an acknowledgement and we are sending it again !
					sendto(sfd, coding, len, 0, (struct sockaddr*)dest, sizeof(struct sockaddr_in6));
				}

				//place packet into senderBuffer
				for(i = 0; i < MAX_WINDOW_SIZE ; i++)
				{
					// we found an empty space in the buffer
					if(senderBuffer[i] == NULL)
					{
						senderBuffer[i] = packet;
						break;
					}
				}

				// erase the buffer we used
				memset(coding, 0, MAX_PACKET_SIZE);

			}
			// if no space in the senderBuffer, we can't send the packet
			// erase content of data we used
			memset(inPut, 0, MAX_PACKET_SIZE);
			size = 0;
			len = MAX_PACKET_SIZE;

		} // end ufds[0]

		// if a packet is received on the socket
		if(ufds[1].revents & POLLIN)
        {
			size = recvfrom(sfd, inPut, len, 0, (struct sockaddr *)dest, &(size_in6));

			if(size < 0)
			{
				fprintf(stderr, "error while reading on socket ...\n" );
			}

			// creating and initializing packet
			packet = pkt_new();
			code = pkt_decode(coding, size, packet);

			// the pacakge is a valid acknowledgement
			if(pkt_get_type(packet) == PTYPE_ACK && code == PKT_OK)
			{
				num = pkt_get_seqnum(packet);
				receiverBufferSize = pkt_get_window(packet);

				//every packet with a sequence number less than num has been accepted by the receiver
				for(i = 0 ; i < MAX_WINDOW_SIZE ; i++)
				{
					if(senderBuffer[i] == NULL)
					{
						// do nothing
					}
					else if(pkt_get_seqnum(senderBuffer[i]) < num)
					{
						// packet was accepted by receiver
						pkt_del(senderBuffer[i]); //delete the packet
						senderBufferSize++;	// more space in buffer
					}
				}
			}
			// delete and re-initalize varables
			size = 0;
			memset(coding, 0, MAX_PACKET_SIZE);
			pkt_del(packet);
		} // ends ufds[1]

		// check if there are packets remaining in the buffer and maybe resend them
		if(senderBufferSize < MAX_WINDOW_SIZE)
		{
			for(i = 0; i < MAX_WINDOW_SIZE ; i++)
			{
				// get the time in seconds
				now = time(NULL);
            	tm = localtime (&now);
            	stamp = tm->tm_sec;

				if(senderBuffer[i] != NULL)
				{
					// timestamp expired, more than 5 sek in buffer
					if(checkTime(pkt_get_timestamp(senderBuffer[i]), stamp) == -1)
					{
						// encode and send the packet
						pkt_encode(senderBuffer[i], coding, &len);
						sendto(sfd, coding, len, 0, (struct sockaddr *)dest, size_in6);
					}
				}
			}
			len = (size_t)MAX_PACKET_SIZE;
			memset(coding, 0, MAX_PACKET_SIZE);
		}

	}// end while loop

}// end sender_loop function

void receiver_loop(int sfd, struct sockaddr_in6 *dest)
{
	int i;
	struct pollfd ufds[1];

    ufds[0].fd = sfd;
    ufds[0].events = POLLIN;
	int rv;

	pkt_t *packet, *ack;
	pkt_t *receiverBuffer[MAX_WINDOW_SIZE];

	for(i = 0; i < MAX_WINDOW_SIZE ; i++)
	{
		receiverBuffer[i] = NULL;
	}

	pkt_status_code code;
	size_t len = MAX_PACKET_SIZE;


	uint8_t senderBufferSize = 1;
	uint8_t receiverBufferSize = MAX_WINDOW_SIZE; // we assume that there is only one free space at the beginning

	char inPut[MAX_PACKET_SIZE];
	char coding[MAX_PACKET_SIZE];

	memset(coding, 0, MAX_PACKET_SIZE);
	memset(inPut, 0, MAX_PACKET_SIZE);

	int size = 0;
	uint8_t seqnum = 0;

	socklen_t size_in6 = sizeof(struct sockaddr_in6);


	while(1)
	{
		//initializing poll to check if something was received (stdin or socket)
		rv = poll(ufds, 1, -1);
      	if(rv == -1)
      	{
          	perror(strerror(errno));
      	}

		// Something is read on socket
		if(ufds[0].revents & POLLIN)
      	{

			// try to read socket
			size = recvfrom(sfd, coding, MAX_PACKET_SIZE, 0, (struct sockaddr*)dest, &size_in6);
			if(size < 0)
			{
				fprintf(stderr, "error while reading socket !\n");
			}

			//SUCCEEDED TO READ SOCKET
			packet = pkt_new();
			code = pkt_decode(coding, size, packet);
			memset(coding, 0, MAX_PACKET_SIZE);
			size = 0;

			// the packet received is the expected one
			if(pkt_get_seqnum(packet) == seqnum && pkt_get_type(packet) == PTYPE_DATA && code == PKT_OK)
			{
				//increment seqnum
				seqnum = incSeqNum(seqnum);
				//set the sender window
				senderBufferSize = pkt_get_window(packet);
				// we printf out the content of the packet
				size = write(fileno(stdout), pkt_get_payload(packet), pkt_get_length(packet));

				if(size < 0)
				{
					fprintf(stderr, "Error while printing content on stdout ... \n");
				}

				size = 0;

				// now we need to send an acknowledgement
				ack = pkt_new();
				pkt_set_type(ack, PTYPE_ACK);
              	pkt_set_window(ack, receiverBufferSize);
              	pkt_set_seqnum(ack, seqnum);

              	pkt_encode(ack, coding, &len);

				sendto(sfd, coding, len, 0, (struct sockaddr*)dest, sizeof(struct sockaddr_in6));

				// we check if there are valid packets in the buffer now
				for(i = 0; i < MAX_WINDOW_SIZE ; i++)
				{
					if(receiverBuffer[i] == NULL)
					{
						// do nothing
					}
					// the packet has the expected  sequence number
					else if(pkt_get_seqnum(receiverBuffer[i]) == seqnum)
					{
						// printf the content out
						size = write(fileno(stdout), pkt_get_payload(receiverBuffer[i]), pkt_get_length(receiverBuffer[i]));
						if(size < 0)
						{
							fprintf(stderr, "Error while printing content on stdout ... \n");
						}
						// packet is deleted
						pkt_del(receiverBuffer[i]);

						// buffersize and seqnum adjusted
						receiverBufferSize--;
						seqnum = incSeqNum(seqnum);

						// re-start at the beginning
						i = 0;
					}
				}
			} // end good packet

			// the received packet is valid but out of sequence
			if(pkt_get_seqnum(packet) != seqnum && pkt_get_type(packet) == PTYPE_DATA && code == PKT_OK)
			{
				// place the packet into the buffer
				for(i = 0; i < MAX_WINDOW_SIZE ; i++)
				{
					// there is some space in the buffer
					if(receiverBuffer[i] == NULL)
					{
						receiverBuffer[i] = packet;
						receiverBufferSize--;
						break;
					}
				}
			}

			// else the packet is not valid

		}// end ufds[0]
	}//end while
}//end function
