#include <stdlib.h>
#include <stdio.h>

int main(int argc, char **argv) {
	
	void *buf = NULL;
	int locked = 0, step = 1024*1024;
	
	while ((buf = malloc(step)) != NULL) locked ++;
	
	fprintf(stderr, "%dMB\n", locked);
	
	return 0;
}
