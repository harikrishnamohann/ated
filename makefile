CC = clang
CFLAGS = -std=gnu17 -lncursesw
TARGET = clem
SRC = src/main.c

all: debug

debug:
	$(CC) $(SRC) $(CFLAGS) -Wall -Wextra -fsanitize=address -g -o $(TARGET)

release:
	$(CC) $(SRC) -O2 $(CFLAGS) -o $(TARGET)

kilo: src/kilo.c
	$(CC) src/kilo.c -Wall -fsanitize=address -Wextra -g -o kilo

.PHONY: debug release
