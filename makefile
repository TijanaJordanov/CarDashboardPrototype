PROGRAM	= dashboard
CC	= gcc
CFLAGS	= -std=c99 -Wall -I/usr/X11R6/include -I/usr/pkg/include 
LDFLAGS	= -L/usr/X11R6/lib -L/usr/pkg/lib
LDLIBS 	= -lglut -lGLU -lGL -lpng -lm -pthread 

$(PROGRAM): dashboard.o rpi_gpio.o log_functions.o
	$(CC) $(LDFLAGS) -o $(PROGRAM) log_functions.o rpi_gpio.o dashboard.o $(LDLIBS) 

.PHONY: clean

clean:
	-rm *.o

