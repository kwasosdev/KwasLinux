CC = gcc
CFLAGS = -Wall -Wextra -O2 -static
TARGET = kwash
SRC = kwash.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

.PHONY: all clean
