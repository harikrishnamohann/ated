CC = clang
CFLAGS = -std=gnu17 -lncursesw
TARGET = laed
SRC = src/main.c

all: debug

debug:
	$(CC) $(SRC) $(CFLAGS) -Wall -fsanitize=address -g -o $(TARGET)

release:
	$(CC) $(SRC) -O2 $(CFLAGS) -o $(TARGET)

.PHONY: debug release run
