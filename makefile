hotpotato: hotpotato.c
	mpicc -o hotpotato hotpotato.c

clean:
	rm -f hotpotato hotpotato.o