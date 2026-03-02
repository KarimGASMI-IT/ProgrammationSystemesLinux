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


CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11
TARGET = simulation

all: $(TARGET)

$(TARGET): main.o
	$(CC) $(CFLAGS) -o $(TARGET) main.o

main.o: main.c
	$(CC) $(CFLAGS) -c main.c

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f *.o $(TARGET)

# Nettoyer les IPC orphelines si la simulation s'est mal terminée
clean_ipc:
	ipcrm -a 2>/dev/null || true

re: clean all

.PHONY: all clean clean_ipc run re
