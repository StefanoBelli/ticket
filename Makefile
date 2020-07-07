FLAGS = -W -Wall -Wextra

all:
	gcc -DPRINT_VALUES -o tktsrv server/linked_list.c server/server.c server/thrmgmt.c -pthread $(FLAGS)
	gcc -o tktcli client.c $(FLAGS)

clean:
	rm -rfv tktsrv tktcli
