CC       := x86_64-w64-mingw32-gcc
CFLAGS   := -nostdlib -Wl,--subsystem,10 -Wl,-e,efi_main \
            -I/usr/include/efi \
            -I/usr/include/efi/x86_64 \
            -I/usr/include/efi/protocol
TARGET   := x86_64/kboot.efi
SRC      := main.c

all: $(TARGET)

$(TARGET): $(SRC)
	@mkdir -p x86_64
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -rf x86_64

.PHONY: all clean