CC = gcc
CFLAGS = -Wall -Wextra -O2 -static
TARGET = init
SRC = init.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

.PHONY: all clean
