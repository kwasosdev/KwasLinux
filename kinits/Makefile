# Динамический подбор компилятора (как в основном скрипте build):
# не на всех системах есть именно gcc.
CC ?= $(shell for c in cc gcc gcc-15 gcc-14 gcc-13 clang; do \
            if command -v $$c >/dev/null 2>&1 && \
               echo 'int main(void){return 0;}' | $$c -x c - -o /tmp/.kinits_cc_test 2>/dev/null; then \
               echo $$c; rm -f /tmp/.kinits_cc_test; break; \
            fi; \
         done)

# Только предупреждения, без -Werror: сборка не должна падать из-за них.
WARNFLAGS = -Wall -Wextra
CFLAGS = $(WARNFLAGS) -O2 -static
TARGET = init
SRC = init.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)
	chmod 755 $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean
