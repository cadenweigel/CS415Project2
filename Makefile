CC = gcc
CFLAGS = -Wall -Wextra -g

PARTS = part1 part2 part3 part4

all: $(PARTS)

part1: part1.c
	$(CC) $(CFLAGS) -o part1 part1.c

part2: part2.c
	$(CC) $(CFLAGS) -o part2 part2.c

part3: part3.c
	$(CC) $(CFLAGS) -o part3 part3.c

part4: part4.c
	$(CC) $(CFLAGS) -o part4 part4.c

clean:
	rm -f $(PARTS)
