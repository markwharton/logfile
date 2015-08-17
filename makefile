build: params.rl parser.rl main.c
	ragel -G2 params.rl
	ragel -G2 parser.rl
	gcc -O2 -lz main.c -o logfile

clean:
	rm -f logfile params.c parser.c
