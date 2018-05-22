

ext2reader: program4.o ext2.o ext2.o
	gcc -o ext2reader program4.o ext2.o


program4.o: program4.c ext2.c ext2.h
	gcc -c program4.c

ext2.o: ext2.c ext2.h
	gcc -c ext2.c

clean:
	rm *.o ext2reader
