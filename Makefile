CC = gcc
CFLAGS = -Wall -Wextra -g
TARGET = tp2

all: $(TARGET)

$(TARGET): tp2.c
	$(CC) $(CFLAGS) -o $(TARGET) tp2.c

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) *.o

clean-ipc:
	ipcrm -a 2>/dev/null || true

.PHONY: all run clean clean-ipc
