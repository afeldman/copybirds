#include <stdio.h>

int main(void) {
	int sli = sizeof(long int);
	int sll = sizeof(long long);
	if (sll >= 8) {
		return 0;
	}
	if (sli <= 4)  {
		printf("Warning: The datatype 'long int' is only %i bytes in size on your platform. Need a size of > 4 bytes to be sure to handle files with a size of more than 2 GB.\n", sli);
		return 1;
	}
	return 0;
}
