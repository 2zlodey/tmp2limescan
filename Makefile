FLAGS =-lLimeSuite -g -lm 

all:
	gcc -o limesdr_debug ./limesdr_dump.c  $(FLAGS)
