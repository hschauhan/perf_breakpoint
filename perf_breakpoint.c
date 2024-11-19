#define _GNU_SOURCE
#include <linux/perf_event.h>    /* Definition of PERF_* constants */
#include <linux/hw_breakpoint.h> /* Definition of HW_* constants */
#include <sys/syscall.h>         /* Definition of SYS_* constants */
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <string.h>

static int gfd;

int setup_bp(bool is_x, void *addr, int sig)
{
	struct perf_event_attr pe;
	int fd;

	memset(&pe, 0, sizeof(struct perf_event_attr));
	pe.type = PERF_TYPE_BREAKPOINT;
	pe.size = sizeof(struct perf_event_attr);

	pe.config = 0;
	pe.bp_type = is_x ? HW_BREAKPOINT_X : HW_BREAKPOINT_W;
	pe.bp_addr = (unsigned long)addr;
	pe.bp_len = sizeof(long);

	pe.sample_period = 1;
	pe.sample_type = PERF_SAMPLE_IP;
	pe.wakeup_events = 1;

	pe.disabled = 1;
	pe.exclude_kernel = 1;
	pe.exclude_hv = 1;

	fd = syscall(SYS_perf_event_open, &pe, 0, -1, -1, 0);
	if (fd < 0) {
		printf("Failed to open event: %llx\n", pe.config);
		return -1;
	}

	fcntl(fd, F_SETFL, O_RDWR|O_NONBLOCK|O_ASYNC);
	fcntl(fd, F_SETSIG, sig);
	fcntl(fd, F_SETOWN, getpid());

	ioctl(fd, PERF_EVENT_IOC_RESET, 0);

	return fd;
}

static void sig_handler_bp(int signum, siginfo_t *oh, void *uc)
{
	ioctl(gfd, PERF_EVENT_IOC_DISABLE, 0);
	printf("Breakpoint is hit!!\n");
}

static void sig_handler_wp(int signum, siginfo_t *oh, void *uc)
{
	ioctl(gfd, PERF_EVENT_IOC_DISABLE, 0);
	printf("Watchpoint is hit!!\n");
}

static void test_func(void)
{
}

int main(int argc, char *argv[])
{
	struct sigaction sa;
	unsigned long test_data;

	if (argc < 2) {
		printf("usage: perf_bp <watchpoint|break>\n");
		exit(0);
	}

	if (!strncmp(argv[1], "break", strlen("break"))) {
		memset(&sa, 0, sizeof(struct sigaction));
		sa.sa_sigaction = (void *)sig_handler_bp;
		sa.sa_flags = SA_SIGINFO;

		if (sigaction(SIGIO, &sa, NULL) < 0) {
			printf("failed to setup signal handler\n");
			return -1;
		}
	
		gfd = setup_bp(1, test_func, SIGIO);

		if (gfd < 0) {
			return -1;
		}

		ioctl(gfd, PERF_EVENT_IOC_ENABLE, 0);

		test_func();

		ioctl(gfd, PERF_EVENT_IOC_DISABLE, 0);

		close(gfd);
	} else if (!strncmp(argv[1], "watchpoint", strlen("watchpoint"))) {
		memset(&sa, 0, sizeof(struct sigaction));
		sa.sa_sigaction = (void *)sig_handler_wp;
		sa.sa_flags = SA_SIGINFO;

		if (sigaction(SIGUSR1, &sa, NULL) < 0) {
			printf("failed to setup signal handler\n");
			return -1;
		}

		gfd = setup_bp(0, &test_data, SIGUSR1);

		if (gfd < 0) {
			printf("Failed to setup watchpoint\n");
			return -1;
		}
		ioctl(gfd, PERF_EVENT_IOC_ENABLE, 0);
		test_data = 0xdeadbeef;
		ioctl(gfd, PERF_EVENT_IOC_DISABLE, 0);
	} else {
		printf("Unknown argument: %s\n", argv[1]);
		exit(-1);
	}

	return 0;
}
