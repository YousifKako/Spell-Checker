CFLAGS = -g

objects = Main.c

Checker : $(objects)
	cc -pthread -o Checker $(objects)

.PHONY : clean
clean :
	rm -pthread Checker $(objects)
