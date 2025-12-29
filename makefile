CC = clang
CFLAGS = -std=gnu17 -lncursesw
TARGET = laed
SRC = src/main.c

all: debug

debug:
	@echo "[Debug Build]"
	$(CC) $(SRC) $(CFLAGS) -Wall -fsanitize=address -g -o $(TARGET)

release:
	@echo "[Release Build]"
	$(CC) $(SRC) -O2 $(CFLAGS) -o $(TARGET)

run: debug
	./$(TARGET)

.PHONY: debug release run
