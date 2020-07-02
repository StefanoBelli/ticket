FLAGS = -W -Wall -Wextra

all:
	gcc -o server server.c $(FLAGS)
	gcc -o client client.c $(FLAGS)

clean:
	rm -rfv server client
