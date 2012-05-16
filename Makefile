CC      = gcc
COMMAND = `pkg-config fuse --cflags --libs`
DEBUG   = -g
OPTIMIZ = 
WARNING = -Wall
INCLUDE = .
LIBCURL = -lcurl
LIBJSON = -ljson

all:
# gcc -Wall -g `pkg-config fuse --cflags --libs` -I. hmac/*.c misc/*.c driver/*.c sdisk.c -lcurl -ljson -o sdisk
	$(CC) $(WARNING) $(DEBUG) $(COMMAND) -I$(INCLUDE) hmac/*.c misc/*.c driver/*.c sdisk.c $(LIBCURL) $(LIBJSON) -o sdisk

clean:
	rm -v .meta output configure/vdisk.map

run:
	./sdisk ~/sdisk

check_running:
	ps aux | grep sdisk
