#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>

int main(int argc, char **argv) {
	
	int sock;
	fprintf(stderr, "Trying to open socket...\n");
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "Can't open socket\n");
		return 5;
	}
	else {
		fprintf(stderr, "Socked opened\n");
	}
	
	return 0;
}
