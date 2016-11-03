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

uint8_t compareSeqNum(uint8_t seqnum1, uint8_t seqnum2)
{
	if(seqnum1 < seqnum2)
	{
		return 0;
	}
	else if(seqnum1 == 255 && seqnum2 == 0)
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

int checkTime(int time1, int time2)
{
	if(time1 == 61)
	{
		return 0;
	}
	//difference is more than 2sec
	if((time2 - time1) > 2 || (time2 - time1) < -2)
	{
		return -1;
	}
	return 0;
}

void sender_loop(int sfd, struct sockaddr_in6 *dest, char const *fname)
{

	// other variables
	int end = FALSE;
	int endFile = FALSE;
	int size = 0;
	int i;

	// buffers for reading stdin and socket
	char inPut[MAX_PAYLOAD_SIZE];
	char coding[MAX_PACKET_SIZE];

	pkt_t *packet;
	size_t len = MAX_PACKET_SIZE;
	socklen_t size_in6 = sizeof(struct sockaddr_in6);
	pkt_status_code code;

	//variables for timestamp
	time_t now;
	struct tm *tm;
	uint32_t stamp = 0;

	//Sequence number starts with 0
	uint8_t seqNum = 0;


	// if you want to change senderBufferSize just find and replace WINDOW with the size you want :)
	pkt_t *senderBuffer[WINDOW];

	for(i = 0; i < WINDOW ; i++)
	{
		senderBuffer[i] = NULL;
	}

	int receiverBufferSize = 1; // we assume that there is only one free space at the beginning
	uint8_t senderBufferSize = WINDOW;

	//we read on both : stdin (or fname) and socket
	struct pollfd ufds[2];
	int rv = 0;

	// file descriptor
	int in_fd = 0;

	// if there is a file, read from it
	if(fname)
	{
		in_fd = open(fname, O_RDONLY);
	}
	else
	{
		in_fd = fileno(stdin);
	}

	ufds[0].fd = in_fd;
	ufds[0].events = POLLIN;
	ufds[1].fd = sfd;
	ufds[1].events = POLLIN;


	//while we read something and buffer is not empty
	while(1)
	{
		//
		if(endFile == TRUE && senderBufferSize == WINDOW)
		{
			break;
		}

		//initializing poll to check if something was received (stdin or socket)
		rv = poll(ufds, 2, 150);

		// error with poll
		if(rv == -1)
		{
			perror(strerror(errno));
			break;
		}

		//something was received on stdin or file was read
		if(ufds[0].revents & POLLIN)
		{
			end = FALSE;
			// if we read a file, we read it until the end
			while(end == FALSE)
			{
				// read stdin or file
				size = read(in_fd, inPut, MAX_PAYLOAD_SIZE);
				fprintf(stderr, "[DEBUG] Size read : [%d]\n",size);

				// if we read on stdin an can send it in one packet we leave the loop
				if(size < MAX_PACKET_SIZE && in_fd == fileno(stdin))
				{
					end = TRUE;
				}

				// end of file
				if(size == 0)
				{
					endFile = TRUE;
					end = TRUE;
					break;
				}

				// there is place in both buffers so we can send packets
				if(senderBufferSize > 0 && receiverBufferSize > 0)
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
					pkt_encode(packet, coding, &len);
					sendto(sfd, coding, len, 0, (struct sockaddr*)dest, sizeof(struct sockaddr_in6));
					fprintf(stderr, "[DEBUG] Packet with seqnum [%u] and window size [%u] send \n", pkt_get_seqnum(packet), pkt_get_window(packet));

					//place packet into senderBuffer
					for(i = 0 ; i < WINDOW ; i++)
					{
						// we found an empty space in the buffer
						if(senderBuffer[i] == NULL)
						{
							senderBuffer[i] = packet;
							fprintf(stderr, "[DEBUG] Packet with seqnum [%u] placed in SenderBuffer[%d] \n", pkt_get_seqnum(packet), i);
							break;
						}
					}
					// erase the buffer we used
					memset(coding, 0, MAX_PACKET_SIZE);
					memset(inPut, 0, MAX_PAYLOAD_SIZE);
				}

				//there is no more place in one of the buffers
				if(senderBufferSize == 0 || receiverBufferSize == 0)
				{
					while(senderBufferSize == 0 || receiverBufferSize == 0)
					{
						rv = poll(ufds, 2, 150);
						// we get something from the receiver

						if(ufds[1].revents & POLLIN)
						{
								size = recvfrom(sfd, coding, len, 0, (struct sockaddr *)dest, &(size_in6));
								// create a new packet
								packet = pkt_new();
								code = pkt_decode(coding, size, packet);
								memset(coding, 0, MAX_PACKET_SIZE);

								// the pacakge is a valid acknowledgement
								if(pkt_get_type(packet) == PTYPE_ACK && code == PKT_OK)
								{
									// set the receiverBuffer
									receiverBufferSize = pkt_get_window(packet);
									fprintf(stderr, "[DEBUG] Acknowledgement with seqnum [%u] received \n", pkt_get_seqnum(packet));
									//every packet with a sequence number less than num has been accepted by the receiver
									for(i = 0 ; i < WINDOW ; i++)
									{
										if(senderBuffer[i] == NULL)
										{
											// do nothing
										}
										else if(compareSeqNum(pkt_get_seqnum(senderBuffer[i]), pkt_get_seqnum(packet)) == 0)
										{
											fprintf(stderr, "[DEBUG] Delete senderBuffer[%d] with seqnum [%u] from senderBuffer\n", i, pkt_get_seqnum(senderBuffer[i]));
											// packet was accepted by receiver
											pkt_del(senderBuffer[i]); //delete the packet
											senderBuffer[i] = NULL;
											senderBufferSize++;	// more space in buffer
										}
									}
								}
						}

						// try to resend packets
						for(i = 0; i < WINDOW ; i++)
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
									fprintf(stderr, "[DEBUG] Resending package with seqnum [%u]\n", pkt_get_seqnum(senderBuffer[i]));
									// encode and send the packet
									pkt_encode(senderBuffer[i], coding, &len);
									sendto(sfd, coding, len, 0, (struct sockaddr *)dest, size_in6);
								}
								len = (size_t)MAX_PACKET_SIZE;
								memset(coding, 0, MAX_PACKET_SIZE);
							}
						}
					}
				}// if senderBuffer == 0

				// check if we have to resend packages
				if(senderBufferSize < WINDOW)
				{

					for(i = 0; i < WINDOW ; i++)
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
								fprintf(stderr, "[DEBUG] Resending package with seqnum [%u]\n", pkt_get_seqnum(senderBuffer[i]));
								// encode and send the packet
								pkt_encode(senderBuffer[i], coding, &len);
								sendto(sfd, coding, len, 0, (struct sockaddr *)dest, size_in6);
							}
							len = (size_t)MAX_PACKET_SIZE;
							memset(coding, 0, MAX_PACKET_SIZE);
						}
					}

				}
			}// end while isReading
		}// end ufds[0]


		// if a packet is received on the socket (acknowledgement)
		if(ufds[1].revents & POLLIN)
		{

			size = recvfrom(sfd, coding, len, 0, (struct sockaddr *)dest, &(size_in6));

			if(size < 0)
			{
				fprintf(stderr, "[DEBUG] Error while reading on socket ...\n" );
			}

			// creating and initializing packet
			packet = pkt_new();
			code = pkt_decode(coding, size, packet);
			memset(coding, 0, MAX_PAYLOAD_SIZE);

			// the pacakge is a valid acknowledgement
			if(pkt_get_type(packet) == PTYPE_ACK && code == PKT_OK)
			{
				receiverBufferSize = pkt_get_window(packet);
				fprintf(stderr, "[DEBUG] Acknowledgement with seqnum [%u] received \n", pkt_get_seqnum(packet));

				//every packet with a sequence number less than num has been accepted by the receiver
				for(i = 0 ; i < WINDOW ; i++)
				{
					if(senderBuffer[i] == NULL)
					{
						// do nothing
					}
					else if(compareSeqNum(pkt_get_seqnum(senderBuffer[i]), pkt_get_seqnum(packet)) == 0)
					{
						fprintf(stderr, "[DEBUG] Delete senderBuffer[%d] with seqnum [%u] from senderBuffer\n", i, pkt_get_seqnum(senderBuffer[i]));
						// packet was accepted by receiver
						pkt_del(senderBuffer[i]); //delete the packet
						senderBuffer[i] = NULL;
						senderBufferSize++;	// more space in buffer
					}
				}
			}
			// delete
			pkt_del(packet);
		} // ends ufds[1]


		// check if there are packets remaining in the buffer and maybe resend them
		if(senderBufferSize < WINDOW)
		{
			for(i = 0; i < WINDOW ; i++)
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
						fprintf(stderr, "[DEBUG] Resending package with seqnum [%u]\n", pkt_get_seqnum(senderBuffer[i]));
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

	if(fname)
	{
		close(in_fd);
	}
	shutdown(sfd, SHUT_WR);

}// end sender_loop function



void receiver_loop(int sfd, struct sockaddr_in6 *dest, const char *fname)
{
	int i;
	struct pollfd ufds[1];

	ufds[0].fd = sfd;
	ufds[0].events = POLLIN;
	int rv;

	pkt_t *packet, *ack;
	pkt_t *receiverBuffer[WINDOW];

	for(i = 0; i < WINDOW ; i++)
	{
		receiverBuffer[i] = NULL;
	}

	pkt_status_code code;
	size_t len = MAX_PACKET_SIZE;

	uint8_t receiverBufferSize = WINDOW; // we assume that there is only one free space at the beginning

	char inPut[MAX_PACKET_SIZE];
	char coding[MAX_PACKET_SIZE];

	memset(coding, 0, MAX_PACKET_SIZE);
	memset(inPut, 0, MAX_PACKET_SIZE);

	int size = 0;
	uint8_t seqnum = 0;

	socklen_t size_in6 = sizeof(struct sockaddr_in6);

	int in_fd = 0;

	// if there is a file, read from it
	if(fname)
	{
		in_fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC, NULL);
	}
	else
	{
		in_fd = fileno(stdout);
	}


	while(1)
	{
		//initializing poll to check if something was received
		rv = poll(ufds, 1, 150);

		if(rv == -1)
		{
			perror(strerror(errno));
			break;
		}

		// Something is read on socket
		if(ufds[0].revents & POLLIN)
		{
			// try to read socket
			size = recvfrom(sfd, coding, MAX_PACKET_SIZE, 0, (struct sockaddr*)dest, &size_in6);

			// error while reading
			if(size < 0)
			{
				fprintf(stderr, "[DEBUG] error while reading socket !\n");
			}

			if(size == 0)
			{
				break;
			}

			//SUCCEEDED TO READ SOCKET
			packet = pkt_new();
			code = pkt_decode(coding, size, packet);
			memset(coding, 0, MAX_PACKET_SIZE);

			// the packet is empty so shut down receiver
			if(pkt_get_length(packet) == 0)
			{
				fprintf(stderr, "[DEBUG] Before breaking because length is 0\n");
				break;
			}
			// the received packet is valid but out of sequence
			if(pkt_get_seqnum(packet) != seqnum && pkt_get_type(packet) == PTYPE_DATA && code == PKT_OK)
			{
				fprintf(stderr, "[DEBUG] Received packet with WRONG seqnum [%u] <=> expected was [%u]\n", pkt_get_seqnum(packet), seqnum);
				// place the packet into the buffer
				for(i = 0; i < WINDOW ; i++)
				{
					// there is some space in the buffer
					if(receiverBuffer[i] == NULL)
					{
						fprintf(stderr, "[DEBUG] Packet with seqnum[%u] stored in receiverBuffer[%d]\n", pkt_get_seqnum(packet), i);
						receiverBuffer[i] = packet;
						receiverBufferSize--;
						break;
					}
				}
			}

			// the packet received is the expected one
			if(pkt_get_seqnum(packet) == seqnum && pkt_get_type(packet) == PTYPE_DATA && code == PKT_OK)
			{
				fprintf(stderr, "[DEBUG] Received packet with EXPECTED seqnum [%u]\n", pkt_get_seqnum(packet));
				//increment seqnum
				seqnum = incSeqNum(seqnum);

				// we printf out the content of the packet
				size = write(in_fd, pkt_get_payload(packet), pkt_get_length(packet));

				fprintf(stderr, "\n");

				if(size < 0)
				{
					fprintf(stderr, "[DEBUG] Error while printing content on stdout ... \n");
				}

				// now we need to send an acknowledgement
				ack = pkt_new();
				pkt_set_type(ack, PTYPE_ACK);
				pkt_set_window(ack, receiverBufferSize);
				pkt_set_seqnum(ack, seqnum);

				pkt_encode(ack, coding, &len);

				sendto(sfd, coding, len, 0, (struct sockaddr*)dest, sizeof(struct sockaddr_in6));
				fprintf(stderr, "[DEBUG] Sending acknowledgement with seqnum [%u]\n", seqnum);

				// we check if there are valid packets in the buffer now
				for(i = 0; i < WINDOW ; i++)
				{
					if(receiverBuffer[i] == NULL)
					{
						// do nothing
					}
					// the packet has the expected  sequence number
					else if(pkt_get_seqnum(receiverBuffer[i]) == seqnum)
					{
						// printf the content out
						size = write(in_fd, pkt_get_payload(receiverBuffer[i]), pkt_get_length(receiverBuffer[i]));
						if(size < 0)
						{
							fprintf(stderr, "[DEBUG] Error while printing content on stdout ... \n");
						}
						// packet is deleted
						pkt_del(receiverBuffer[i]);
						fprintf(stderr, "[DEBUG] Deleted packet with seqnum [%u] from receiverBuffer[%d]\n", pkt_get_seqnum(receiverBuffer[i]), i);
						receiverBuffer[i] = NULL;

						// buffersize and seqnum adjusted
						receiverBufferSize++;
						seqnum = incSeqNum(seqnum);

						// re-start at the beginning
						i = 0;
					}
				}
			} // end good packet

			// else the packet is not valid

		}// end ufds[0]
	}//end while

	//close the file
	if(fname)
	{
		close(in_fd);
	}

}//end function
