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

#include "thrmgmt.h"
#include "malloc_utils.h"

#ifndef DATETIME_FORMAT
#define DATETIME_FORMAT "%Y/%m/%d %H:%M:%S"
#endif

#ifndef DEFAULT_PORT
#define DEFAULT_PORT 8123
#endif

#ifndef DEFAULT_THREADS
#define DEFAULT_THREADS 1024
#endif

#ifndef DEFAULT_RCVTO
#define DEFAULT_RCVTO 3
#endif

#define thrmgmt_strerror_loge_exit(r) \
{ \
	if(r != THRMGMT_OK) { \
		char buf[256]; \
		thrmgmt_strerror(r, buf, 256); \
		loge(buf); \
		exit(EXIT_FAILURE); \
	} \
}

#define strerror_log(msg) \
{ \
	char buf[256] = { 0 }; \
	snprintf(buf, 256, "%s: %s", msg, strerror(errno)); \
	loge(buf); \
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
#define VERBOSE if(conf(__verbose__))

#define conf(prop) (g_conf.prop)

//typedefs and prototypes

typedef unsigned long long ulong64;
typedef unsigned int uint32;
typedef unsigned char ubyte;
typedef unsigned short ushort16;
typedef long long int64;
typedef char* (*svcop_handler_fpt)(const char*, const char*);

typedef struct {
	char* name;
	ubyte has_arg;
	svcop_handler_fpt handler;
	uint32 len;
} svcop;

typedef struct {
	ubyte booked;
	uint32 unique_code;
} seat;

typedef struct {
	ubyte __verbose__;
	uint32 rows;
	uint32 pols;
	uint32 n_total_seats;
	uint32 n_threads; //default threads
	uint32 rcvtos; //default rcvtos
	uint32 rcvmaxbuf;
	uint32 sndavailseatbuf;
	int listen_sd;
} program_instance_config;

void request_handler(void*);
char* op_get_available_seats(const char*, const char*);
char* op_book_seats(const char*, const char*);
char* op_revoke_booking(const char*, const char*);

//global variables

seat** g_seats = NULL;

program_instance_config g_conf = 
{ 0, 0, 0, 0, DEFAULT_THREADS, DEFAULT_RCVTO, 0, 0, 0 };

#define NOPS 3
const svcop g_op_listing[NOPS] = 
{
	{ "GetAvailableSeats", 0, (svcop_handler_fpt) op_get_available_seats, 17 },
	{ "BookSeats", 1, op_book_seats, 9 },
	{ "RevokeBooking", 1, op_revoke_booking, 13 }
};

// program aux functions
//
int dgt(uint32 j) {
	int i = 0;

	while (j) {
		j /= 10;
		i++;
	}

	return i;
}

int itos(uint32 n, char* out) {
	int slen = dgt(n);
	for (int i = slen - 1; n; --i) {
		out[i] = (n % 10) + 48;
		n /= 10;
	}

	return slen;
}

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
	
	close(conf(listen_sd));

	VERBOSE log("giving every thread chance to terminate gracefully...");

	int semv;
	thrmgmt_waitall(&semv);
	if((uint32) semv < conf(n_threads)) {
		conf(rcvtos) <<= 1;
		char buf[256] = { 0 };
		snprintf(buf, 256, "waiting %d seconds before ending cleanup procedure...", conf(rcvtos));
		log(buf);
		sleep(conf(rcvtos));
	}

	thrmgmt_finish();

	for(unsigned i = 0; i < conf(rows); ++i) {
		malloc_free(g_seats[i]);
	}

	malloc_free(g_seats);

	VERBOSE log("bye");
	exit(res);
}

void print_usage_exit(const char* first) {
	fprintf(stderr, "usage: %s [-v | --verbose] [-t th | --nthreads th] [-o to | --recvto to]"
			" [-l po| --port po] [-r nr | --rows nr] [-p np | --pols np]\n", first);
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
	while((client_sd = accept(conf(listen_sd), (struct sockaddr*) &addr, &len)) > 0) {
		VERBOSE {
			char buf[256] = { 0 };
			snprintf(buf, 256, "accepted connection from %s:%d", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
			log(buf);
		}

		struct timeval tv;
		tv.tv_sec = conf(rcvtos);
		tv.tv_usec = 0;

		if(setsockopt(client_sd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval)) == 0) {
			int rv;
			while((rv = thrmgmt_dispatch_work(request_handler, (void*) client_sd)) == THRMGMT_DISPATCH_WORK_RETRY) {
				log("failed to dispatch \"work\", retrying");
			}

			thrmgmt_strerror_loge_exit(rv);
		} else
			strerror_log("setsockopt(SO_RCVTIMEO)");
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
			conf(rows) = (uint32) r;

		} else if(arg(argv[i], "--pols", "-p")) {
			ulong64 p;
			get_ullong_value_for_option(argv, &p, i);
			conf(pols) = (uint32) p;

		} else if(arg(argv[i], "--port", "-l")) {
			ulong64 l;
			get_ullong_value_for_option(argv, &l, i);
			use_port = (ushort16) l;

		} else if(arg(argv[i], "--recvto", "-o")) {
			ulong64 o;
			get_ullong_value_for_option(argv, &o, i);
			conf(rcvtos) = (uint32) o;

		} else if(arg(argv[i], "--verbose", "-v")) {
			conf(__verbose__) = 1;

		} else if(arg(argv[i], "--nthreads", "-t")) {
			ulong64 t;
			get_ullong_value_for_option(argv, &t, i);
			conf(n_threads) = (uint32) t;

		} else {
			if(i > 0)
				printf("ignoring unrecognized option: %s\n", argv[i]);

		}
	}

	if(conf(rows) == 0 || conf(pols) == 0 || conf(rcvtos) == 0 || conf(n_threads) == 0) {
		print_usage_exit(argv[0]);
	}

#ifdef PRINT_VALUES
	VERBOSE { 
		char buf[256] = { 0 };
		snprintf(buf, sizeof(buf), "verbose = true, rows = %d, pols = %d, use_port = %d, n_threads = %d, rcvtos = %d", 
				conf(rows), conf(pols), use_port, conf(n_threads), conf(rcvtos));
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

	conf(listen_sd) = get_new_listening_socket(use_port);
	if(conf(listen_sd) < 0) {
		loge("unable to create new listening socket");
		exit(EXIT_FAILURE);
	}

	g_seats = (seat**) calloc(conf(rows), sizeof(seat*));
	malloc_check_exit_on_error(g_seats);

	for(unsigned i = 0; i < conf(rows); ++i) {
		g_seats[i] = (seat*) calloc(conf(pols), sizeof(seat));
		malloc_check_exit_on_error(g_seats[i]);
	}

	int thr_init_res = thrmgmt_init(conf(n_threads));
	thrmgmt_strerror_loge_exit(thr_init_res);
	
	VERBOSE log("thrmgmt initialization done");

	signal(SIGINT, cleanup_exit);
	signal(SIGTERM, cleanup_exit);

	if(sigprocmask(SIG_UNBLOCK, &blocked_signals, NULL) < 0) {
		strerror_log("sigprocmask(SIG_UNBLOCK)");
		exit(EXIT_FAILURE);
	}

	VERBOSE log("unblocked signals");


	/* ITA: buffer più grandi
	 * (recv) BookSeatsx1,x2,y1,y2,z1,z2,...,k1,k2\r\n
	 * (send) x1,x2,y1,y2,z1,z2,...,k1,k2\0
	 * 
	 * rcvmaxbuf:
	 *  9 = len("BookSeats")
	 *  2 = len("\r\n")
	 *  20 * n_total_seats = 2 * 10 * n_total_seats = 2 * len(32_bit_integer) * n_total_seats
	 *  n_total_seats * 2 - 1 = # di virgole necessarie
	 *
	 * sndavailseatbuf:
	 *    rcvmaxbuf - 10 = non abbiamo 9 + 2: len("BookSeats") + len("\r\n"), ma dobbiamo inviare
	 *    il terminatore '\0', è quindi equivalente a rcvmaxbuf - 11 + 1
	 *  
	 *  E' conveniente fare cosi perchè il buffer è fisso, niente realloc(s), una sola recv.
	 *  E' inverosimile che una sala cinema abbia talmente tanti posti tali da ottenere buffer
	 *  enormi.
	 */

	conf(n_total_seats) = conf(rows) * conf(pols);
	conf(rcvmaxbuf) = 11 + (20 * conf(n_total_seats)) + ((conf(n_total_seats) << 1) - 1);
	conf(sndavailseatbuf) = conf(rcvmaxbuf) - 10;

	VERBOSE {
		char buf[256] = { 0 };
		snprintf(buf, 256, 
				"max receive buffer size: %dB\n"
				"max command GetAvailableSeats send buffer size: %dB\n"
				"receive timeout: %ds\n"
				"listening on port %d\n"
				"setup done, waiting for connections...", 
				conf(rcvmaxbuf), conf(sndavailseatbuf), conf(rcvtos), use_port);
		log(buf);
	}

	//cleanup_exit will never be reached at this point, unless accept() fails
	cleanup_exit(handle_connections()); 
}

#define NOT_FOUND -1

int64 detect_request_termination(const char* req, uint32 len) {
	for(uint32 i = 0; i < len - 1; ++i) {
		uint32 i_plus_one = i + 1;
		if(req[i] == '\r' && req[i_plus_one] == '\n')
			return i_plus_one; //found terminator
	}

	return NOT_FOUND;
}

svcop_handler_fpt request_parsereq(const char* reqstr, uint32 end, char **out_argstrt_ptr) {
	*out_argstrt_ptr = NULL;

	for(int i = 0; i < NOPS; ++i) {
		if(strncmp(reqstr, g_op_listing[i].name, g_op_listing[i].len) == 0) {
			if(g_op_listing[i].has_arg) {
				if(end == g_op_listing[i].len + 1)
					return NULL; //no argument, but argument is required

				*out_argstrt_ptr = (char*) reqstr + g_op_listing[i].len;
			}

			return g_op_listing[i].handler; //OK
		}
	}

	return NULL; //NOT FOUND
}

void request_handler(void* _sd) {
	int sd = (int) _sd;

	char *request = (char*) calloc(conf(rcvmaxbuf), sizeof(char));
	malloc_check_exit_on_error(request);

	int termpos = NOT_FOUND;

/* --- recv --- */
	int err = 0;
intr_retry:
	err = recv(sd, request, conf(rcvmaxbuf), 0);
	if(err < 0) {
		if(errno == EINTR)
			goto intr_retry;
		else if(errno) {
			if(errno != EWOULDBLOCK)
				strerror_log("recv");
			goto request_finish;
		}
	} else if(err == 0) {
		VERBOSE log("client suddenly closed connection");
		goto request_finish;
	}
/* --- recv --- */

	termpos = detect_request_termination(request, err);
	if(termpos == NOT_FOUND) {

/* --- send --- */	
intr1_retry:
		if(send(sd, "Op:invalid\r\n\0", sizeof("Op:invalid\r\n"), MSG_NOSIGNAL) < 0) {
			if (errno == EINTR)
				goto intr1_retry;
			else
				strerror_log("send");
		}
/* --- send --- */
		
		goto request_finish;
	}

	char* arg_starts_from_ptr = NULL;
	svcop_handler_fpt target_op = request_parsereq(request, termpos, &arg_starts_from_ptr);
	
	if(target_op == NULL) {
		
/* --- send --- */	
intr2_retry:
		if(send(sd, "Op:invalid\r\n\0", sizeof("Op:invalid\r\n"), MSG_NOSIGNAL) < 0) {
			if (errno == EINTR)
				goto intr2_retry;
			else
				strerror_log("send");
		}
/* --- send --- */
		
		goto request_finish;
	}

	char* endpos = request + termpos;
	*(endpos - 1) = 0;

	char* ans = target_op(arg_starts_from_ptr, endpos);
/* --- send --- */
intr3_retry:
	if(send(sd, ans, strlen(ans) + 1, MSG_NOSIGNAL) < 0) {
		if (errno == EINTR)
			goto intr3_retry;
		else
			strerror_log("send");
	}
/* --- send --- */

	malloc_free(ans);
	
request_finish: 
	close(sd);
	malloc_free(request);
}

char* op_get_available_seats(const char* __unused_1__, const char* __unused_2__) {
	(void)__unused_1__;
	(void)__unused_2__;

	char comma = ',';

	char* res = (char*) calloc(conf(sndavailseatbuf), sizeof(char));
	malloc_check_exit_on_error(res);

	uint32 len = 0;

	for(uint32 i = 0; i < conf(rows); ++i) {
		char sip1[11] = { 0 };
		int sip1_len = itos(i + 1, sip1);

		for(uint32 j = 0; j < conf(pols); ++j) {
			ubyte is_booked = g_seats[i][j].booked;

			if(is_booked == 0) {
				char sjp1[11] = { 0 };
				int sjp1_len = itos(j + 1, sjp1);

				strncat(res, sip1, sip1_len);
				strncat(res, &comma, 1);
				strncat(res, sjp1, sjp1_len);
				strncat(res, &comma, 1);

				len += 2 + sip1_len + sjp1_len;
			}
		}
	}

	if(len > 0)
		res[len - 1] = 0;

	return res;
}

#define book_seats_error(msg, msglen) { \
		malloc_free(to_book); \
		char* err = (char*) malloc(sizeof(char) * msglen); \
		malloc_check_exit_on_error(err); \
		memcpy(err, msg, msglen); \
		return err; } 
		
char* op_book_seats(const char* arg, const char* endat) {
	uint32 n_compo = 0;

	uint32 n_bookings = 0;
	seat** to_book = (seat**) malloc(sizeof(seat*) * 1);
	malloc_check_exit_on_error(to_book);

	char* tok = strtok((char*)arg, ",");
	while(tok && tok < endat) {
		++n_compo;

		if(n_compo % 2 == 1) {
			char* prevtok = tok;
			tok = strtok(NULL, ",");
			
			if(tok == NULL)
				book_seats_error("Fail:noteven", 13);

			uint32 x;
			uint32 y;
			
			int res1, res2;
			if((res1 = stoull(prevtok, (ulong64*) &x)) || (res2 = stoull(tok, (ulong64*) &y)) || 
					x == 0 || y == 0 || x > conf(rows) || y > conf(pols))
				book_seats_error("Fail:exceed", 12);

			if(n_bookings + 1 > conf(n_total_seats))
				book_seats_error("Fail:toomuch", 13);

			to_book[n_bookings] = &g_seats[x - 1][y - 1];
			++n_bookings;
			
			to_book = (seat**) realloc(to_book, sizeof(seat*) * (n_bookings + 1));
			malloc_check_exit_on_error(to_book);
		}

		tok = strtok(NULL, ",");
	}

	if(n_bookings == 0)
		book_seats_error("Fail:wholeempty", 16);

	for(uint32 i = 0; i < n_bookings; ++i) {
		if(to_book[i]->booked == 1)
			book_seats_error("Fail:notavail", 14);
	}

	uint32 unique = (uint32) time(NULL);

	for(uint32 i = 0; i < n_bookings; ++i) {
		to_book[i]->booked = 1;
		to_book[i]->unique_code = unique;
	}

	malloc_free(to_book);

	char code[11] = { 0 };
	uint32 code_len = itos(unique, code);

	int len = code_len + 9;
	char* res = (char*) malloc(sizeof(char) * len);
	malloc_check_exit_on_error(res);
	memcpy(res, "Success:", 8);
	memcpy(res + 8, code, code_len);
	res[len - 1] = 0;

	return res;
}

#undef book_seats_error

char* op_revoke_booking(const char* arg, const char* endat) {
	return NULL;
}

