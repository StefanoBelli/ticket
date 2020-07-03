#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <limits.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

#ifndef DATETIME_FORMAT
#define DATETIME_FORMAT "%Y/%M/%d %H:%m:%S"
#endif

#ifndef DEFAULT_PORT
#define DEFAULT_PORT 8123
#endif

#define strerror_log(msg) \
{ \
	char buf[256] = { 0 }; \
	snprintf(buf, 256, "%s: %s", msg, strerror(errno)); \
	loge(buf); \
}

#define malloc_check(m) \
{ \
	if(m == NULL) { \
		perror("malloc"); \
		exit(EXIT_FAILURE); \
	} \
}


#define malloc_free(m) \
{ \
	if(m) { \
		free(m); \
		m = NULL; \
	} \
}

#define stoull_exit(a, s) \
{ \
	int err; \
	if((err = stoull(a, s))) { \
		printf("%s: not a valid integer or of/uf occoured\n", \
				a); \
		exit(EXIT_FAILURE); \
	} \
}

#define value_check(target_idx, cur_idx, opt) \
{ \
	target_idx = cur_idx + 1; \
	if(target_idx == argc) { \
		printf("%s: missing value\n", opt); \
		print_usage_exit(argv[0]); \
	} \
}

#define get_ullong_value_for_option(arg_array, out_value_ptr, arg_cur_idx) \
{ \
	int next_idx; \
	value_check(next_idx, i, argv[i]); \
	stoull_exit(argv[next_idx], out_value_ptr); \
	++arg_cur_idx; \
}

#define arg(a, s, l) (strcmp(a, s) == 0 || strcmp(a, l) == 0)

#define log(msg) (basic_log("LOG", msg))
#define loge(msg) (basic_log("ERROR", msg))
#define VERBOSE if(__verbose__)


// type definitions
typedef struct {
	int x;
	int y;
} pair;

typedef struct {
	unsigned code;
	unsigned n_seats;
	pair *seats;
} ticket;

typedef unsigned long long ulong64;
typedef unsigned int uint32;
typedef unsigned char ubyte;
typedef unsigned short ushort16;

//global variables
ubyte **free_seats = NULL;
ticket *tickets = NULL;
ubyte __verbose__ = 0;
uint32 rows = 0;
uint32 pols = 0;
uint32 n_tickets = 0;
int listen_sd;

// program aux functions

//int stoull(__in const char*, __out ulong64*);
// returns 0 on success, 1 on failure
int stoull(const char* s, ulong64* res) {
	char* end = NULL;
	*res = strtoull(s, &end, 10);
	return *end != 0 || errno;
}

void basic_log(const char* type, const char* msg) {
	char timebuf[256] = { 0 };
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);

	if(tm == NULL) {
		snprintf(timebuf, 256, "localtime: %s", strerror(errno));
	} else {
		if(strftime(timebuf, sizeof(timebuf), DATETIME_FORMAT, tm) == 0)
			snprintf(timebuf, 256, "strftime: %s", strerror(errno));
	}

	FILE* out;
	if(strcmp(type, "ERROR") == 0) {
		out = stderr;
	} else if(strcmp(type, "LOG") == 0) {
		out = stdout;
	} else {
		fprintf(stderr, "DEBUG -- INVALID LOG TYPE: %s\nDEBUG -- Exiting...\n",type);
		exit(EXIT_FAILURE);
	}

	char* msgln = strtok((char*) msg, "\n");
	while(msgln) {
		fprintf(out, "%s [%s]: %s\n", type, timebuf, msgln);
		msgln = strtok(NULL, "\n");
	}
}

void cleanup_exit(int res) {
	VERBOSE log("cleaning up...");

	for(unsigned i = 0; i < rows; ++i) {
		malloc_free(free_seats[i]);
	}

	malloc_free(free_seats);

	close(listen_sd);

	VERBOSE log("bye");
	exit(res);
}

void print_usage_exit(const char* first) {
	fprintf(stderr, "usage: %s [-v | --verbose] [-l po| --port po] [-r nr | --rows nr] [-p np | --pols np]\n", first);
	exit(EXIT_FAILURE);
}

int get_new_listening_socket(ushort16 port) {
	int sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(sd < 0) {
		VERBOSE strerror_log("socket");
		return -1;
	}

	struct sockaddr_in addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);

	int ret = bind(sd, (struct sockaddr*) &addr, sizeof(struct sockaddr_in));
	if(ret < 0) {
		VERBOSE strerror_log("bind");
		return -1;
	}

	int val = 1;
	if(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (void*)&val, sizeof(int)) < 0) {
		VERBOSE strerror_log("setsockopt");
		return -1;
	}

	if(listen(sd, 0) < 0) {
		VERBOSE strerror_log("listen");
		return -1;
	}

	return sd;
}

void sigrcv(int sig) {
	((void)sig);
	cleanup_exit(EXIT_SUCCESS);
}

//end program aux functions

int handle_connections() {
	struct sockaddr_in addr = { 0 };
	socklen_t len = sizeof(struct sockaddr_in);

	int client_sd;
	while((client_sd = accept(listen_sd, (struct sockaddr*) &addr, &len)) > 0) {
		//READ req AND WRITE res
		VERBOSE {
			char buf[256] = { 0 };
			snprintf(buf, 256, "accepted connection from %s:%d", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
			log(buf);
		}

		close(client_sd);
	}

	if(client_sd < 0) {
		VERBOSE strerror_log("accept");
		loge("error on accepting connections");
		return EXIT_FAILURE;
	}

	//will never be reached, if handle_connection() returns, it means that accept() failed
	return EXIT_SUCCESS;
}

int main(int argc, char** argv) {
	ushort16 use_port = DEFAULT_PORT;

	for(int i = 0; i < argc; ++i) {
		if(arg(argv[i], "--rows", "-r")) {
			ulong64 r;
			get_ullong_value_for_option(argv, &r, i);
			rows = (uint32) r;

		} else if(arg(argv[i], "--pols", "-p")) {
			ulong64 p;
			get_ullong_value_for_option(argv, &p, i);
			pols = (uint32) p;

		} else if(arg(argv[i], "--port", "-l")) {
			ulong64 l;
			get_ullong_value_for_option(argv, &l, i);
			use_port = (ushort16) l;

		} else if(arg(argv[i], "--verbose", "-v")) {
			__verbose__ = 1;

		} else {
			if(i > 0)
				printf("ignoring unrecognized option: %s\n", argv[i]);

		}
	}

	if(rows == 0 || pols == 0) {
		print_usage_exit(argv[0]);
	}

#ifdef PRINT_VALUES
	VERBOSE { 
		char buf[256] = { 0 };
		snprintf(buf, sizeof(buf), "verbose = true, rows = %d, pols = %d, use_port = %d", 
				rows, pols, use_port);
		log(buf);
	}
#endif

	VERBOSE log("starting setup...");

	sigset_t blocked_signals;
	sigfillset(&blocked_signals);
	if(sigprocmask(SIG_BLOCK, &blocked_signals, NULL) < 0) {
		strerror_log("sigprocmask(SIG_BLOCK)");
		exit(EXIT_FAILURE);
	}

	VERBOSE log("blocked signals");

	listen_sd = get_new_listening_socket(use_port);
	if(listen_sd == -1) {
		loge("unable to create new listening socket");
		exit(EXIT_FAILURE);
	}

	free_seats = (ubyte**) calloc(rows, sizeof(ubyte*));
	malloc_check(free_seats);

	for(unsigned i = 0; i < rows; ++i) {
		free_seats[i] = (ubyte*) calloc(pols, sizeof(ubyte));
		malloc_check(free_seats[i]);
	}

	signal(SIGINT, cleanup_exit);
	signal(SIGTERM, cleanup_exit);

	if(sigprocmask(SIG_UNBLOCK, &blocked_signals, NULL) < 0) {
		strerror_log("sigprocmask(SIG_UNBLOCK)");
		exit(EXIT_FAILURE);
	}

	VERBOSE log("unblocked signals");

	VERBOSE {
		char buf[256] = { 0 };
		snprintf(buf, 256, "listening on port %d\nsetup done, waiting for connections...", use_port);
		log(buf);
	}

	//cleanup_exit will never be reached at this point, unless accept() fails
	cleanup_exit(handle_connections()); 
}
