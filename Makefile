CC = gcc
CFLAGS = -Wall -Wextra -g -pthread

OBJS = main.o dispatcher.o counters.o stats.o worker_pool.o

hw2: $(OBJS)
	$(CC) $(CFLAGS) -o hw2 $(OBJS)

%.o: %.c hw2.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJS) hw2
