#include "syscall.h"
#include "synchop.h"

int main() {
	int *a, x, b=12345;
	a = (int *)sys_ShmAllocate(sizeof(int));
	x = sys_Fork();
	if(x == 0) {
		sys_PrintString("Main hu bachcha \n");
		*a = 10;
		sys_PrintInt(*a);
		sys_PrintChar('\n');
		b= 12178;
		sys_PrintString("Value of b is :");
		sys_PrintInt(b);
		sys_PrintChar('\n');
	}
	else {
		sys_Join(x);
		sys_PrintString("main hu Bapu\n");
		sys_PrintInt(*a);
		sys_PrintString(": bachche ka kaam hai bhaiya\n");
		sys_PrintInt(b);

	}
	sys_PrintString("I go on forever\n");
	return 0;
}
