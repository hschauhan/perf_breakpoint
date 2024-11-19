CC=${CROSS_COMPILE}gcc
PROGRAM=perf_breakpoint
SRCS=perf_breakpoint.c
OBJS=$(SRCS:.c=.o)
LDFLAGS=
CFLAGS=

$(PROGRAM): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY:
clean:
	rm -rf $(OBJS) $(PROGRAM) $~
