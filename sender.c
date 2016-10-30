#include <string.h>
#include <stdlib.h>
#include "packet_interface.h"
#include "transport_interface.h"

int main(int argc, char *argv[]){
	if(argc < 3)
	{
		fprintf(stderr, "Not engough arguments");
		return -1;
	}
	char *filename = NULL;
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

	}

	if(real_address(destName, &destAdd) != 0)
	{
			fprintf(stderr, "error while converting address");
	}

	sfd = create_socket(NULL, -1, &destAdd, port);

	if(sfd < 0)
	{
		fprintf(stderr, "could not create socket");
	}

	// if no file send data from stdin
	if(filename == NULL)
	{
		sender_loop(sfd, &destAdd);
	}

	return EXIT_SUCCESS;

}
