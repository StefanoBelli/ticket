FLAGS = -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -W -Wall -Wextra

all:
	gcc -DPOSIX_VERSION -o tktsrv server/server.c server/thrmgmt.c -pthread $(FLAGS)
	gcc -o tktcli client.c $(FLAGS)

clean:
	rm -rfv tktsrv tktcli
