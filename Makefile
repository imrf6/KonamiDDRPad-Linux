CFLAGS = -Wall
CFLAGS += -Wextra
CFLAGS += -Wno-unused-parameter
CFLAGS += -Wno-unused-but-set-variable
CFLAGS += -O0
CFLAGS += -g
CFLAGS += `pkg-config --cflags hidapi-libusb`
LIBS = `pkg-config --libs hidapi-libusb`

all:
	${CC} ${CFLAGS} src/main.c -o KonamiDDRPadLinux ${LIBS}
