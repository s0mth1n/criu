#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>

#include <linux/futex.h>
#include <sys/syscall.h>
#include <string.h>
#include <sys/mman.h>

#include "zdtmtst.h"

const char *test_doc = "Check that a process-shared robust mutex in a MAP_SHARED file is not poisoned with FUTEX_OWNER_DIED when tasks are killed after dump";
const char *test_author = "B Srinivas Reddy <sriniv4sreddy@gmail.com>";

char *filename;
TEST_OPTION(filename, string, "file name", 1);

struct shared_state {
	pthread_mutex_t mutex;
	int lock_ret;
	volatile pid_t worker_tid;
};

static struct shared_state *state;

static int worker_is_sleeping(pid_t tid)
{
	char path[64], buf[512], *rp;
	int fd, n;
	char state_ch = '?';

	snprintf(path, sizeof(path), "/proc/self/task/%d/stat", tid);
	fd = open(path, O_RDONLY);
	if (fd == -1)
		return 0;
	n = read(fd, buf, sizeof(buf) - 1);
	close(fd);
	if (n <= 0)
		return 0;
	buf[n] = '\0';
	rp = strrchr(buf, ')');
	if (rp && rp[1] == ' ')
		state_ch = rp[2];
	return state_ch == 'S' || state_ch == 'D';
}

/* wait until the worker is blocked in the kernel's futex wait */
static int wait_worker_blocked(void)
{
	int i;

	for (i = 0; i < 100000; i++) {
		if (state->worker_tid && worker_is_sleeping(state->worker_tid))
			return 0;
		usleep(100);
	}
	return -1;
}

static void *worker_fn(void *arg)
{
	int ret;

	state->worker_tid = syscall(SYS_gettid);
	ret = pthread_mutex_lock(&state->mutex);
	state->lock_ret = ret;
	if (ret == 0)
		pthread_mutex_unlock(&state->mutex);
	return NULL;
}

int main(int argc, char **argv)
{
	pthread_t worker;
	pthread_mutexattr_t attr;
	int fd;

	test_init(argc, argv);

	fd = open(filename, O_CREAT | O_RDWR, 0600);
	if (fd == -1) {
		pr_perror("open");
		return 1;
	}
	if (ftruncate(fd, 4096)) {
		pr_perror("ftruncate");
		return 1;
	}

	state = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED, fd, 0);
	if (state == MAP_FAILED) {
		pr_perror("mmap");
		return 1;
	}

	if (pthread_mutexattr_init(&attr) ||
	    pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST) ||
	    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) ||
	    pthread_mutex_init(&state->mutex, &attr)) {
		pr_perror("mutex init");
		return 1;
	}
	pthread_mutexattr_destroy(&attr);

	/* the main thread holds the mutex across checkpoint/restore */
	if (pthread_mutex_lock(&state->mutex)) {
		pr_perror("pthread_mutex_lock");
		return 1;
	}

	state->lock_ret = -1;
	if (pthread_create(&worker, NULL, worker_fn, NULL)) {
		pr_perror("pthread_create");
		return 1;
	}

	if (wait_worker_blocked()) {
		fail("worker did not block on the mutex");
		return 1;
	}

	test_daemon();
	test_waitsig();

	pthread_mutex_unlock(&state->mutex);
	if (pthread_join(worker, NULL)) {
		pr_perror("pthread_join");
		return 1;
	}

	if (state->lock_ret != 0) {
		fail("worker woke with %d (%s), expected 0", state->lock_ret,
		     state->lock_ret == EOWNERDEAD ? "EOWNERDEAD" : "error");
		return 1;
	}

	munmap(state, 4096);
	close(fd);

	pass();
	return 0;
}
