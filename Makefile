FLAGS = -W -Wall -Wextra
COMMON_DEFINES = -DPOSIX_VERSION

all:
	gcc -o tktsrv server/server.c server/thrmgmt.c \
		-pthread -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast \
		$(COMMON_DEFINES) $(FLAGS)
	gcc -o tktcli client.c $(COMMON_DEFINES) $(FLAGS)

clean:
	rm -rfv tktsrv tktcli
