CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11
TARGET = simulation

all: $(TARGET)

$(TARGET): main.o
        $(CC) $(CFLAGS) -o $(TARGET) main.o

main.o: main.c
        $(CC) $(CFLAGS) -c main.c

clean:
        rm -f *.o $(TARGET)

re: clean all

.PHONY: all clean re
