CFLAGS=-c -Wall -O3 -DDEBUG_LOG
#CFLAGS=-c -Wall -O3 

# For Raspberry Pi, it's recommended to use PIGPIO for fast SPI I/O
#LIBS = -lpigpio -pthread
# For other boards, use the Linux SPI driver
LIBS = 

all: clearscreen fbtext fbbmp

clearscreen: clearscreen.o 
	$(CC) clearscreen.o $(LIBS) -o clearscreen

fbtext: fbtext.o 
	$(CC) fbtext.o $(LIBS) -o fbtext

fbbmp:	fbbmp.o
	$(CC) fbbmp.o $(LIBS) -o fbbmp


fbtext.o: fbtext.c
	$(CC) $(CFLAGS) fbtext.c

clearscreen.o: clearscreen.c
	$(CC) $(CFLAGS) clearscreen.c

fbbmp.o: fbbmp.c
	$(CC) $(CFLAGS) fbbmp.c

clean:
	rm *.o clearscreen fbtext fbbmp

