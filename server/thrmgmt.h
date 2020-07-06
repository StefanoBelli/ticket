#ifndef THRMGMT_H
#define THRMGMT_H

#define THRMGMT_OK 0

#define THRMGMT_INIT_INVAL 4
#define THRMGMT_INIT_MALLOC_FAILURE 5
#define THRMGMT_INIT_SEMINIT_FAILURE 6

#define THRMGMT_WAITALL_JOIN_FAILURE 9
#define THRMGMT_WAITALL_SEMREL_FAILURE 10
#define THRMGMT_WAITALL_BOTH_FAILURE 11

#define THRMGMT_DISPATCH_WORK_SEMWAIT_FAILURE 17
#define THRMGMT_DISPATCH_WORK_TRYJOIN_FAILURE 18
#define THRMGMT_DISPATCH_WORK_MALLOC_FAILURE 19
#define THRMGMT_DISPATCH_WORK_CREATE_FAILURE 20
#define THRMGMT_DISPATCH_WORK_RETRY 21

typedef void(*work_routine_fpt)(void*);

/* 
 * thrmgmt_init
 * 
 * DESCRIZIONE: 
 *		iniziallizza lo stato interno, necessaria prima di qualunque altra chiamata
 *		è responsabilità dell'utente eseguirla, il modulo non eseguirà alcun check
 *		
 * NOTA BENE:
 *		max_running_threads > 0
 *
 * RITORNA: 
 *    * THRMGMT_OK se tutto è andato a buon fine
 *    * uno degli errori della classe THRMGMT_INIT_* altrimenti
 */
int thrmgmt_init(unsigned max_running_threads);

/*
 * thrmgmt_dispatch_work
 * 
 * DESCRIZIONE:
 *		assegna un "lavoro" a un thread. Solo se vi sono risorse disponibili, vale a dire
 *		il valore dei token del semaforo è maggiore di 0. Altrimenti il thread chiamante
 *		viene messo in stato di blocco.
 *
 * RITORNA:
 *		* THRMGMT_OK se tutto è andato a buon fine
 *		* uno degli errori della classe THRMGMT_DISPATCH_WORK_* altrimenti
 */
int thrmgmt_dispatch_work(work_routine_fpt routine, void* args);

/* 
 * thrmgmt_waitall
 *
 * DESCRIZIONE:
 *		attende che tutti i thread siano terminati
 *
 * RITORNA:
 *		* THRMGMT_OK se tutto è andato a buon fine
 *		* uno degli errori della classe THRMGMT_WAITALL_* altrimenti
 */
int thrmgmt_waitall(int *semval);

/*
 * thrmgmt_finish
 *		libera completamente le risorse allocate, ogni errore è ignorato
 */
void thrmgmt_finish();

void thrmgmt_strerror(int error, char* dst, int dst_size);

#endif
