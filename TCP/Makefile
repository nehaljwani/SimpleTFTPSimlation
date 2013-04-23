CC = gcc
CFLAGS = -Wall
PROG = Assignment 

SRCS = Assignment.c
LIBS = -lssl -lcrypto 

all: $(PROG)

$(PROG):	$(SRCS)
	  $(CC) $(CFLAGS) -o $(PROG) $(SRCS) $(LIBS)

clean:
	 rm -f $(PROG)
