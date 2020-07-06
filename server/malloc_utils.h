#ifndef MALLOC_UTILS_H
#define MALLOC_UTILS_H

#define malloc_check_exit_on_error(m) \
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

#endif
