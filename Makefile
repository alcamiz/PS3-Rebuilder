CC=gcc
CFLAGS=-O1 -I include
WFLAGS=-Wno-incompatible-pointer-types
LDFLAGS=-lz -lmbedcrypto -lcurl
SOURCES=main.c src/ird.c src/iso.c src/sfo.c src/net.c src/fault.c src/util.c src/cwalk.c
EXECUTABLE=ps3_rebuild

all:
	$(CC) $(CFLAGS) $(WFLAGS) $(SOURCES) $(LDFLAGS) -o $(EXECUTABLE)
clean:
	rm -rf $(EXECUTABLE)
