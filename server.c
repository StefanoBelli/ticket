#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#ifndef DATETIME_FORMAT
#define DATETIME_FORMAT "%Y/%M/%d %H:%m:%S"
#endif

#define malloc_check(m) \
	if(m == NULL) { \
		perror("malloc"); \
		exit(EXIT_FAILURE); }

#define malloc_free(m) \
	if(m) { \
		free(m); \
		m = NULL; \
	}

#define arg(a, s, l) (strcmp(a, s) == 0 || strcmp(a, l) == 0)

#define log(msg) (basic_log("LOG", msg))
#define loge(msg) (basic_log("ERROR", msg))
#define VERBOSE if(__verbose__)

#define strbool(b) (b ? "true" : "false")

#define stol_exit(a, s) \
	int err; \
	if((err = stol(a, s))) { \
		printf("strtol: %s\n", strerror(errno)); \
		exit(EXIT_FAILURE); \
	}

#define value_check(target_idx, cur_idx, opt) \
	target_idx = cur_idx + 1; \
	if(target_idx == argc) { \
		printf("%s: missing value\n", opt); \
		print_usage_exit(argv[0]); \
	}

#define get_value_for_option(arg_array, out_value_ptr) {\
	int next_idx; \
	value_check(next_idx, i, argv[i]); \
	stol_exit(argv[next_idx], out_value_ptr); }


// type definitions
struct pair {
	int x;
	int y;
};

struct active_tickets {
	unsigned code;
	struct pair *seats;
};

//global variables
int **free_seats = NULL;
char __verbose__ = 0;

// program aux functions
int stol(const char* s, unsigned* res) {
	char* end = NULL;
	*res = strtol(s, &end, 10);

	return errno;
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

void print_usage_exit(const char* first) {
	fprintf(stderr, "usage: %s [-v | --verbose] [-r nr | --rows nr] [-p np | --pols np]\n", first);
	exit(EXIT_FAILURE);
}
//end program aux functions

int main(int argc, char** argv) {
	unsigned rows = 0;
	unsigned pols = 0;

	for(int i = 0; i < argc; ++i) {
		if(arg(argv[i], "--rows", "-r")) {
			get_value_for_option(argv, &rows);
		} else if(arg(argv[i], "--pols", "-p")) {
			get_value_for_option(argv, &pols);
		} else if(arg(argv[i], "--verbose", "-v")) {
			__verbose__ = 1;
		}
	}

	if(rows == 0 || pols == 0) {
		 print_usage_exit(argv[0]);
	}

	VERBOSE { 
		char buf[256] = { 0 };
		snprintf(buf, sizeof(buf), "verbose = %s, rows = %d, pols = %d\nperforming setup...", strbool(__verbose__), rows, pols);
		log(buf);
	}

	free_seats = (int**) calloc(rows, sizeof(int*));
	malloc_check(free_seats);

	for(unsigned i = 0; i < rows; ++i) {
		free_seats[i] = (int*) calloc(pols, sizeof(int));
		malloc_check(free_seats[i]);
	}
	
	VERBOSE log("cleaning up...");

	for(unsigned i = 0; i < rows; ++i) {
		malloc_free(free_seats[i]);
	}
	
	malloc_free(free_seats);

	VERBOSE log("bye");

	return EXIT_SUCCESS;
}
