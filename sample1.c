#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>

#define HPS	0x100000UL
#define VADDR	0x700000000000UL
#define PROT	PROT_READ|PROT_WRITE

#define MADV_WIPEONFORK	18		/* Zero memory on fork, child only */
#define MADV_KEEPONFORK	19		/* Undo MADV_WIPEONFORK */

int main() {
	int ret;
	char *mem = mmap((void *)VADDR, HPS , PROT, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	printf("mapped address: %p\n", mem);

	memcpy(mem, "test1\0", 6);

	if (fork()) { /* parent */
		printf("parent (%d), [%s]\n", getpid(), mem);
	} else { /* child */
		printf("child 1 (%d), [%s]\n", getpid(), mem);
		return 0;
	}

	usleep(100000);

	ret = madvise(mem, HPS, MADV_WIPEONFORK);
	printf("madvise(MADV_WIPEONFORK) returned %d\n", ret);

	if (!fork()) { /* child */
		printf("child 2 (%d), [%s]\n", getpid(), mem);
		return 0;
	}

	usleep(100000);

	ret = madvise(mem, HPS, MADV_KEEPONFORK);
	printf("madvise(MADV_KEEPONFORK) returned %d\n", ret);

	if (!fork()) { /* child */
		printf("child 3 (%d), [%s]\n", getpid(), mem);
		return 0;
	}

	usleep(1000000);
}
