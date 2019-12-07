#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#define HPS	0x200000UL
#define VADDR	0x700000000000UL
#define PROT	PROT_READ|PROT_WRITE

#define MADV_WILLNEED	3		/* will need these pages */
#define MADV_DONTNEED	4		/* don't need these pages */
#define MADV_FREE	8		/* free pages only if memory pressure */
#define MADV_REMOVE	9		/* remove these pages & resources */
#define MADV_COLD	20		/* deactivate these pages */
#define MADV_PAGEOUT	21		/* reclaim these pages */

#define err(x) perror(x),exit(EXIT_FAILURE)

void check_smaps(int i) {
	char cmd[256];
	sprintf(cmd, "cat /proc/%d/smaps | grep -A 20 ^%lx > /tmp/smaps%d", getpid(), VADDR, i);
	system(cmd);
}

void show_diff_smaps(int i, int j) {
	char cmd[256];
	sprintf(cmd, "diff -u0 /tmp/smaps%d /tmp/smaps%d", i, j);
	system(cmd);
}

int main(int argc, char *argv[]) {
	int ret;
	int fd = -1;
	int mapflag;
	int madvflag;
	char *mem;

	if (argc > 1)
		mapflag = strtol(argv[1], NULL, 0);
	if (argc > 2)
		madvflag = strtol(argv[2], NULL, 0);

	switch(mapflag) {
	case 0:
		mapflag = MAP_PRIVATE|MAP_ANONYMOUS; 
		break;
	case 1:
		mapflag = MAP_SHARED|MAP_ANONYMOUS; 
		break;
	case 2:
		mapflag = MAP_PRIVATE;
		break;
	case 3:
		mapflag = MAP_SHARED;
		break;
	case 4:
		break;
	}

	/* printf("%x\n", mapflag & MAP_ANONYMOUS); */
	if (!(mapflag & MAP_ANONYMOUS)) {
		fd = open("./test.tmp", O_CREAT|O_RDWR, 0755);
		if (fd == -1)
			err("open");
	}
	printf("mapflag: %x, madvflag: %x, fd: %d\n", mapflag, madvflag, fd);

	mem = mmap((void *)VADDR, HPS , PROT, mapflag, fd, 0);
	if (mem == (void *)MAP_FAILED)
		err("mmap");
	printf("mapped address: %p\n", mem);

	// fault in
	memcpy(mem, "test1\0", 6);
	check_smaps(1);
	ret = madvise(mem, HPS, madvflag);
	printf("madvise(%d) returned %d\n", madvflag, ret);
	check_smaps(2);
	printf("mem: [%s]\n", mem);
	check_smaps(3);

	puts("---");
	show_diff_smaps(1, 2);
	puts("---");
	show_diff_smaps(2, 3);
}
