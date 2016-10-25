#include <string.h>
#include <stdlib.h>
#include "packet_interface.h"
#include "create_socket.h"

int main(int argc, const char *argv[]){
	if(argc < 3)
		printf("sender <hostname/IPv6> <port> \n");

	// getting the dest addr
	struct sockaddr_in6 *dest_val = malloc(sizeof(struct sockaddr_in6));
	const char *hostname = real_address(argv[1], dest_val);
	// assertion
	if(hostname)
		printf("%s\n",hostname);

	// getting the dest port
	int dst_port = 0;
	dst_port = (int) strtol(argv[2], NULL, 10);

	// sender will be at 0:0:0:0:0:0:0:1 (loopback address)
	struct sockaddr_in6 *src_val = malloc(sizeof(struct sockaddr_in6));
	const char *localhost = real_address("::1", src_val);
	int src_port = 1;
	// assertion
	if(localhost)
		printf("%s\n",localhost);

	int ret = create_socket(src_val, src_port, dest_val, dst_port);
	if(ret < 0) return ret;

	

	free(dest_val);
	free(src_val);
	return 0;
}
