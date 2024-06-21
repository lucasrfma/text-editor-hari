br: build run

build: ./src/hari.c
	$(CC) ./src/hari.c -o ./out/hari -Wall -Wextra -pedantic -std=c99

run: ./out/hari
	./out/hari

key: ./src/key_ver.c
	$(CC) ./src/key_ver.c -o ./out/key_ver -Wall -Wextra -pedantic -std=c99
	./out/key_ver