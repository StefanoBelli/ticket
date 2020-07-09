#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#ifndef DEFAULT_PORT
#define DEFAULT_PORT 8123
#endif

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
	value_check(next_idx, i, arg_array[i]); \
	stoull_exit(arg_array[next_idx], out_value_ptr); \
	++arg_cur_idx; \
}

#define arg(a, l, s) (strcmp(a, l) == 0 || strcmp(a, s) == 0)

typedef unsigned int uint32;
typedef unsigned long long ulong64;
typedef unsigned short ushort16;

void print_usage_exit(const char* fa) {
	printf("usage: %s [ --host ht | -h ht ] [ --port pt | -p pt ] [ --unique ue | -u ue ]\n", fa);
	exit(EXIT_FAILURE);
}

void remove_char(char *str, char garbage) {
    char *src, *dst;
    for (src = dst = str; *src != '\0'; src++) {
        *dst = *src;
        if (*dst != garbage) dst++;
    }
    *dst = '\0';
}

char* replace_char(char* str, char find, char replace){
    char *current_pos = strchr(str,find);
    while (current_pos){
        *current_pos = replace;
        current_pos = strchr(current_pos,find);
    }
    return str;
}

//int stoull(__in const char*, __out ulong64*);
// returns 0 on success, 1 on failure
int stoull(const char* s, ulong64* res) {
	char* end = NULL;
	*res = strtoull(s, &end, 10);
	return *end != 0;
}

struct sockaddr_in host_lookup(const char* hostname, ushort16 port) {// 1) lookup
	struct sockaddr_in addr;
	addr.sin_family = 0;

	struct hostent* host_entity = gethostbyname(hostname);
	if(host_entity == NULL)
		return addr;

	addr.sin_family = AF_INET;
	addr.sin_addr = *(struct in_addr*) host_entity->h_addr;
	addr.sin_port = htons(port);

	return addr;
}

#define SOCKET_ERROR -1
#define CONNECT_ERROR -2

int get_connected_socket(struct sockaddr_in* addr) {
	int sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(sd < 0)
		return SOCKET_ERROR;

	if(connect(sd, (struct sockaddr*) addr, sizeof(struct sockaddr_in)) < 0) {
		close(sd);
		return CONNECT_ERROR;
	}

	return sd;
}

#define attempt_connection(newsckfd, hstaddr) \
	int newsckfd = get_connected_socket(hstaddr); \
	if(newsckfd == SOCKET_ERROR) { \
		perror("socket"); \
		return; \
	} else if(newsckfd == CONNECT_ERROR) { \
		perror("connect"); \
		return; \
	}

void print_available_seats(struct sockaddr_in* addr) {
	attempt_connection(sd, addr);

	int err;

intr_write_retry:
	if((err = write(sd, "GetAvailableSeats\r\n", sizeof("GetAvailableSeats\r\n"))) < 0) {
		if(errno == EINTR)
			goto intr_write_retry;
		else {
			perror("write");
			goto finish;
		}
	}

	printf("Available seats listing\n"
		   "=======================\n");
	char buf[1025] = { 0 };
intr_recv_retry:
	while((err = recv(sd, buf, 1024, MSG_NOSIGNAL)) > 0) {
		int i = 0;
		char* tok = strtok(buf, ",");
		while(tok) {
			if(i % 2 == 0) {
				printf("\nseat = ");
			}
			printf("%s: %s ", (i % 2 == 0) ? "row": "pol", tok);
			fflush(stdout);
			++i;
			tok = strtok(NULL, ",");
		}
	}

	if(err < 0) {
		if(errno == EINTR)
			goto intr_recv_retry;
		else {
			perror("read");
			goto finish;
		}
	}

finish:
	puts("\n\n=======================\n");
	close(sd);
}

void book_seats(struct sockaddr_in* addr, char* seats_coords) {
	attempt_connection(sd, addr);
	replace_char(seats_coords, ' ', ',');
	remove_char(seats_coords, '(');
	remove_char(seats_coords, ')');

	int seats_coords_len = strlen(seats_coords);
	int len = seats_coords_len + 11;
	char* req = (char*) calloc(len, sizeof(char));
	if(req == NULL)
		exit(EXIT_FAILURE);

	memcpy(req, "BookSeats", 9);
	memcpy(req + 9, seats_coords, seats_coords_len);
	memcpy(req + 9 + seats_coords_len, "\r\n", 2);

	int err;

intr_write_retry:
	if((err = write(sd, req, len)) < 0) {
		if(errno == EINTR)
			goto intr_write_retry;
		else {
			perror("write");
			goto finish;
		}
	}

	char buf[1025] = { 0 }; //più che abbastanza, il server non può rispondere più di 19 bytes
intr_recv_retry:
	err = recv(sd, buf, 1024, MSG_NOSIGNAL);
	if(err < 0) {
		if(errno == EINTR)
			goto intr_recv_retry;
		else {
			perror("read");
			goto finish;
		}
	}

	puts("Seat booking\n"
		 "============\n");
	
	char* ans = strtok(buf, ":");
	if(strcmp(ans, "Success") == 0)
		printf("You did it! Here's your code: %s\n", strtok(NULL, ":"));
	else {
		//printf("Something went wrong - error: %s\n", strtok(NULL, ":"));
		char* error_string = strtok(NULL, ":");
		if(strcmp(error_string, "notavail") == 0)
			printf("One or more of your chosen seats already booked\n");
		else if(strcmp(error_string, "exceed") == 0)
			printf("Exceeding in terms of # seats or coordinates\n"
					"e.g. if we have rows=3 and pols=4 we can't have (4,5) as coordinate\n"
					"e.g.2 if we have rows=3 * pols=4 12 total seats we can't exceed that"
					"number of booking\n");
	}

	puts("\n============\n");
finish:
	close(sd);
	free(req);
}

void revoke_booking(struct sockaddr_in* addr, const char* unique_code) {
	attempt_connection(sd, addr);

	int unique_code_len = strlen(unique_code);
	int req_len = 15 + unique_code_len;
	
	char* req = (char*) calloc(req_len, sizeof(char));
	if(req == NULL)
		exit(EXIT_FAILURE);

	memcpy(req, "RevokeBooking", 13);
	memcpy(req + 13, unique_code, unique_code_len);
	memcpy(req + 13 + unique_code_len, "\r\n", 2);

	int err;

intr_write_retry:
	if((err = write(sd, req, req_len)) < 0) {
		if(errno == EINTR)
			goto intr_write_retry;
		else {
			perror("write");
			goto finish;
		}
	}
	
	char buf[1025] = { 0 };
intr_recv_retry:
	err = recv(sd, buf, 1024, MSG_NOSIGNAL);
	if(err < 0) {
		if(errno == EINTR)
			goto intr_recv_retry;
		else {
			perror("read");
			goto finish;
		}
	}
	
	puts("Revoke previous booking by unique code\n"
		 "======================================\n");
	
	char* ans = strtok(buf, ":");
	if(strcmp(ans, "Success") == 0)
		puts("Correctly revoked!");
	else {
		ans = strtok(NULL, ":");
		if(strcmp(ans, "nounique") == 0)
			puts("Oops, we couldn't find any code matching yours!");
		else
			puts("Server doesn't like any code which is not-a-number");
	}

	puts("\n======================================\n");

finish:
	close(sd);
	free(req);
}

#define MAX_LINE 1024

#define read_stdin(bufname) \
	char bufname[MAX_LINE] = { 0 }; \
	fgets(bufname, MAX_LINE, stdin); \
	char *pos; \
	if ((pos=strchr(bufname, '\n')) != NULL) \
		*pos = '\0';

int main(int argc, char** argv) {
	ushort16 port = DEFAULT_PORT;
	char* host = "127.0.0.1";

	for(int i = 0; i < argc; ++i) {
		if(arg(argv[i], "--host", "-h")) {
			int i_plus_one;
			value_check(i_plus_one, i, argv[i]);
			i = i_plus_one;
			host = argv[i_plus_one];
			
		} else if(arg(argv[i], "--port", "-p")) {
			ulong64 r;
			get_ullong_value_for_option(argv, &r, i);
			port = (ushort16) r;
			
		}  else {
			if(i > 0)
				printf("ignoring unrecognized option: %s\n", argv[i]);

		}
	}

	printf("resolving %s:%d...\n", host, port);
	struct sockaddr_in host_address = host_lookup(host, port);
	if(host_address.sin_family == 0) {
		printf("unable to resolve \"%s\"\n",host);
		return EXIT_FAILURE;
	}

	while(1) {
		int opt;
		printf("--- options:\n"
				"\t1) Get list of available seats\n"
				"\t2) Book one or more seats\n"
				"\t3) Revoke a previous booking (unique code needed)\n"
				"\t4) Exit\n\nchoice: ");
		fflush(stdout);

		read_stdin(bufopt);
		puts("***");

		if(stoull(bufopt, (ulong64*) &opt) == 0) {
			if(opt < 1 || opt > 4)
				printf("unrecognized option: %d\n", opt);

			else if(opt == 1)
				print_available_seats(&host_address);

			else if(opt == 2) {
				printf("seat coordinates (x1,y1) (x2,y2) ... : ");
				fflush(stdout);
				read_stdin(bufcoords);
				book_seats(&host_address, bufcoords);

			} else if(opt == 3) {
				printf("unique code: ");
				fflush(stdout);
				read_stdin(bufunique);
				revoke_booking(&host_address, bufunique);

			} else if(opt == 4)
				exit(EXIT_SUCCESS);
		} else {
			printf("invalid character for base 10\n");
		}
	}
}

