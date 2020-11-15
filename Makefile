CC = g++
CFLAGS = -Wall

extended_barber: extended_barber.o
	$(CC) $(CFLAGS) extended_barber.o -o extended_barber -lpthread

clean:
	rm *.o extended_barber
