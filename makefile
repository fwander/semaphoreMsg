LIB = -lpthread
CC = gcc
RM = rm
RM += -f
binaries = life-c addem-c

addem-c: addem.c
	$(CC) addem.c -o addem-c $(LIB)

life-c: life.c
	$(CC) life.c -o life-c $(LIB)

clean:
	$(RM) $(binaries)

