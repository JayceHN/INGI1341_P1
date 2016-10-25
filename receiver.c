#include "packet_interface.h"
#include "create_socket.h"
#include <stdio.h>
#include <stdlib.h>


int main(int argc, char *argv[])
{
		if(argc < 3)
		{
			fprintf(stderr, "Not engough arguments");
			return -1;
		}
		char *filename;
		char *destName;
		unsigned int port;
		int sfd = 0;

		struct sockaddr_in6 destAdd;
		memset(&destAdd, 0, sizeof(struct sockaddr_in6));

		// first argument is -f
		if(strcmp(argv[1], "-f") == 0)
		{
				filename = argv[2];
				destName = argv[3];
				port = atoi(argv[4]);
		}

		//first argument is hostname
		else
		{
				destName = argv[1];
				port = atoi(argv[2]);

				// checks if there is a filename too
				if(strcmp(argv[3], "-f") == 0)
				{
						filename = argv[4];
				}
		}

		if(real_address(destName, &destAdd) != NULL)
		{
				fprintf(stderr, "error while converting address");
		}

		sfd = create_socket(&destAdd, port, NULL, -1);

		if(sfd > 0 && wait_for_client(sfd) < 0)
		{
			fprintf(stderr, "could not connect");
		}
		if(sfd < 0)
		{
			fprintf(stderr, "could not create socket");
		}

		read_write_loop(sfd);

		fprintf(stderr, "reveiver END !");
		return EXIT_SUCCESS;




}
