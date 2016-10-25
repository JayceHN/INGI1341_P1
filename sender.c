#include <string.h>
#include <stdlib.h>
#include "packet_interface.h"
#include "create_socket.h"

int main(int argc, const char *argv[]){
	if(argc < 3)
		printf("sender <hostname/IPv6> <port> \n");

	// getting the dest addr
	struct sockaddr_in6 *rval = malloc(sizeof(struct sockaddr_in6));
	const char *hostname = real_address(argv[1], rval);
	// assertion
	if(!hostname)
		printf("invalid hostname/IPv6\n");

	// getting the dest port
	int dst_port = 0;
	dst_port = (int) strtol(argv[2], NULL, 10);

	// sender will be at 0:0:0:0:0:0:0:1 (loopback address)
	const char *localhost = "::1";
	int src_port = 1;

	int ret = create_socket(localhost, src_port, hostname, dst_port);


	free(rval);
}
