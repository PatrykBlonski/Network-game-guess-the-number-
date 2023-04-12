CC=gcc
CFLAGS=-I. -lpthread -lm
#CFLAGS=

OBJECTS = client server
all: $(OBJECTS)

$(OBJECTS):%:%.c
	@echo Compiling $<  to  $@
	$(CC) -o $@ $< $(CFLAGS)

	
clean:
	rm  $(OBJECTS) 
