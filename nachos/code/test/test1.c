#include "syscall.h"

int main()
{
	int *array = (int*)sys_ShmAllocate(2*sizeof(int));
	array[0] = 1;
	array[1] = 2;
	printf("\n%d %d", array[0], array[1]);
	return 0;
}
