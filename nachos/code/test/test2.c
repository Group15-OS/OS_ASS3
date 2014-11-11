#include "syscall.h"
#include "synchop.h"

int main() {
	int  x, semid, condid;
	int seminit = 1;
	int *a = (int *)sys_ShmAllocate(2*sizeof(int));
		a[0] = 10;
		sys_PrintInt(*a);
	semid = sys_SemGet(1234);
	//condid = sys_CondGet(1234);
	//sys_PrintString("Sem id: ");
	//sys_PrintInt(semid);
	//sys_PrintString("\n Cond Id: ");
	//sys_PrintInt(condid);
	//sys_PrintString("SYNCH_SET follows\n");
	//sys_PrintInt(sys_SemCtl(semid, SYNCH_SET, &seminit));	
	x = sys_Fork();
	if(x == 0) {
	//	a[0] = a[0] + 5;
		sys_SemOp(semid, -1);
		sys_PrintString("\nMain hu bachcha ");
		//a[0] = 12;
		sys_PrintInt(a[0]);
		sys_PrintChar('\n');
		sys_SemOp(semid, 1);
	}
	else {
		//sys_Join(x);
		//sys_Sleep(1000);
		sys_SemOp(semid, -1);
		sys_PrintString("\nMain hu Bapu ");
		a[0] = a[0]+1;
		sys_PrintInt(a[0]);
	//sys_PrintString(": bachche ka kaam hai bhaiya\n");
		sys_SemOp(semid, 1);
		sys_Join(x);

	}
//	sys_PrintString("I go on forever\n");
	return 0;
}
