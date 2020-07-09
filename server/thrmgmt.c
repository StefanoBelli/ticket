/* thrmgmt.c - thread management
	ITA: ricerca lineare di un thread libero basato su semaphores
		per indicare disponibilità di thread liberi, 
		non si vuole eccedere un numero di thread massimo, 
		stabilito (eventualmente) dall'utente.

	In caso di risorse esaurite, la chiamata di richiesta di dispatch
	blocca il thread chiamante, finchè almeno una delle risorse non
	viene rilasciata.
*/

#define _GNU_SOURCE //pthread_tryjoin_np

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

#include "malloc_utils.h"
#include "thrmgmt.h"

typedef struct {
	work_routine_fpt perform_work;
	void* user_args;
} __thrmgmt_args;

/* NOT exposed */
static pthread_t* worker_thread;
static sem_t sem_running_threads;
static unsigned max_threads;

static void* __thrmgmt_internal_routine(void* _args) {
	__thrmgmt_args *args = (__thrmgmt_args*) _args;
	args->perform_work(args->user_args);

	malloc_free(args);
	sem_post(&sem_running_threads);
	
	return NULL; 
}


/* exposed */
int thrmgmt_init(unsigned max_running_threads) {
	if(max_running_threads == 0)
		return THRMGMT_INIT_INVAL;

	if((worker_thread = (pthread_t*) calloc(max_running_threads, sizeof(pthread_t))) == NULL)
		return THRMGMT_INIT_MALLOC_FAILURE;

	if(sem_init(&sem_running_threads, 0, max_running_threads) < 0) {
		malloc_free(worker_thread);
		return THRMGMT_INIT_SEMINIT_FAILURE;
	}

	max_threads =  max_running_threads;

	return THRMGMT_OK;
}

int thrmgmt_dispatch_work(work_routine_fpt routine, void* args) {
	//sem_post will not be error-checked

	if(sem_wait(&sem_running_threads) < 0)
		return THRMGMT_DISPATCH_WORK_SEMWAIT_FAILURE;

	for(unsigned i = 0; i < max_threads; ++i) {
		if(worker_thread[i] && pthread_tryjoin_np(worker_thread[i], NULL) < 0 && errno != EBUSY) {
			sem_post(&sem_running_threads);
			return THRMGMT_DISPATCH_WORK_TRYJOIN_FAILURE;
		} else {
			__thrmgmt_args *internal_args = (__thrmgmt_args*) malloc(sizeof(__thrmgmt_args));
			if(internal_args == NULL) {
				sem_post(&sem_running_threads);
				return THRMGMT_DISPATCH_WORK_MALLOC_FAILURE;
			}

			internal_args->perform_work = routine;
			internal_args->user_args = args;

			if(pthread_create(&worker_thread[i], NULL, __thrmgmt_internal_routine, (void*) internal_args) < 0) {
				sem_post(&sem_running_threads);
				return THRMGMT_DISPATCH_WORK_CREATE_FAILURE;
			} else
				return THRMGMT_OK; /*RETURN: OK */
		}	
	}

	sem_post(&sem_running_threads);
	return THRMGMT_DISPATCH_WORK_RETRY;
}

int thrmgmt_waitall(int* semval) {
	int joining_errors = 0;
	int semrel_errors = 0;

	for(unsigned i = 0; i < max_threads; ++i) {
		if(worker_thread[i]) {
			if(pthread_join(worker_thread[i], NULL) < 0)
				joining_errors = 1;
		}
	}

	joining_errors |= (semrel_errors << 1);

	sem_getvalue(&sem_running_threads, semval);
	return joining_errors ? joining_errors & 8 : 0;
}

//errors ignored
void thrmgmt_finish() {
	malloc_free(worker_thread);
	sem_destroy(&sem_running_threads);
}

int thrmgmt_mutex_init(thrmgmt_system_mutex mtx) {
	if(pthread_mutex_init((pthread_mutex_t*)mtx, NULL) < 0)
		return THRMGMT_MUTEX_INIT_FAILURE;
	
	return THRMGMT_OK;
}

int thrmgmt_mutex_lock(thrmgmt_system_mutex mtx) {
	if(pthread_mutex_lock((pthread_mutex_t*)mtx) < 0)
		return THRMGMT_MUTEX_LOCK_FAILURE;
	
	return THRMGMT_OK;
}

int thrmgmt_mutex_unlock(thrmgmt_system_mutex mtx) {
	if(pthread_mutex_unlock((pthread_mutex_t*)mtx) < 0)
		return THRMGMT_MUTEX_UNLOCK_FAILURE;
	
	return THRMGMT_OK;
}

int thrmgmt_mutex_destroy(thrmgmt_system_mutex mtx) {
	if(pthread_mutex_destroy((pthread_mutex_t*)mtx) < 0)
		return THRMGMT_MUTEX_DESTROY_FAILURE;
	
	return THRMGMT_OK;
}

void thrmgmt_strerror(int error, char* dst, int dst_max_size) {
	int current_errno = errno;
	memset(dst, 0, dst_max_size);
	switch(error) {
		case THRMGMT_INIT_MALLOC_FAILURE:
			snprintf(dst, dst_max_size, "thrmgmt_init:malloc: %s",strerror(current_errno));
			break;
		case THRMGMT_INIT_SEMINIT_FAILURE:
			snprintf(dst, dst_max_size, "thrgmtm_init:sem_init: %s", strerror(current_errno));
			break;
		case THRMGMT_INIT_INVAL:
			memcpy(dst, "thrmgmt_init: Invalid argument", sizeof("thrmgmt_init: Invalid argument"));
			break;

		case THRMGMT_DISPATCH_WORK_CREATE_FAILURE: 
			snprintf(dst, dst_max_size, "thrmgmt_dispatch_work:pthread_create: %s", strerror(current_errno));
			break;
		case THRMGMT_DISPATCH_WORK_RETRY:  
			memcpy(dst, "thrmgmt_dispatch_work: retry", sizeof("thrmgmt_dispatch_work: retry"));
			break;
		case THRMGMT_DISPATCH_WORK_MALLOC_FAILURE: 
			snprintf(dst, dst_max_size, "thrmgmt_dispatch_work:malloc: %s", strerror(current_errno));
			break;
		case THRMGMT_DISPATCH_WORK_TRYJOIN_FAILURE:
			snprintf(dst, dst_max_size, "thrmgmt_dispatch_work:pthread_tryjoin_np: %s", strerror(current_errno));
			break;
		case THRMGMT_DISPATCH_WORK_SEMWAIT_FAILURE:
			snprintf(dst, dst_max_size, "thrmgmt_dispatch_work:sem_wait: %s", strerror(current_errno));
			break;
	
		case THRMGMT_WAITALL_BOTH_FAILURE: 
			snprintf(dst, dst_max_size, 
					"thrmgmt_waitall:both thread joining and semaphore token release failed");
			break;
		case THRMGMT_WAITALL_JOIN_FAILURE: 
			snprintf(dst, dst_max_size, "thrmgmt_waitall:pthread_join: %s", strerror(current_errno));
			break;
		case THRMGMT_WAITALL_SEMREL_FAILURE: 
			snprintf(dst, dst_max_size, "thrmgmt_waitall:sem_post: %s", strerror(current_errno));
			break;
		
		case THRMGMT_MUTEX_INIT_FAILURE:
			snprintf(dst, dst_max_size, "thrmgmt_mutex_init:pthread_mutex_init: %s",strerror(current_errno));
			break;
		case THRMGMT_MUTEX_LOCK_FAILURE:
			snprintf(dst, dst_max_size, "thrmgmt_mutex_lock:pthread_mutex_lock: %s",strerror(current_errno));
			break;
		case THRMGMT_MUTEX_UNLOCK_FAILURE:
			snprintf(dst, dst_max_size, "thrmgmt_mutex_unlock:pthread_mutex_unlock: %s",strerror(current_errno));
			break;
		case THRMGMT_MUTEX_DESTROY_FAILURE:
			snprintf(dst, dst_max_size, "thrmgmt_mutex_destroy:pthread_mutex_destroy: %s",strerror(current_errno));
			break;

		default:
			snprintf(dst, dst_max_size, "thrmgmt: Success");
	}
}
