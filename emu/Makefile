# TB-32 in C -- kopflose Fassung zum Vergleichen mit Python
CC      = cc
CFLAGS  = -O2 -Wall -Wextra -std=c99
ZIEL    = tb32

$(ZIEL): cpu.c machine.c main.c tb32.h
	$(CC) $(CFLAGS) -o $(ZIEL) cpu.c machine.c main.c

sauber:
	rm -f $(ZIEL)
