br: build run

build: ./src/hari.c
	$(CC) ./src/hari.c -o ./out/hari -Wall -Wextra -pedantic -std=c99

run: ./out/hari
	./out/hari
