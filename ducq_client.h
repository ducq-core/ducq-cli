#ifndef _DUCQ_CLIENT_
#define _DUCQ_CLIENT_

#include <ducq_log.h>

typedef int (*log_f)(void *logger, enum ducq_log_level level, const char *fmt, ...);

struct client_config {
	const char  *host;
	const char  *port;
	const char  *command;
	const char  *route;
	const char  *payload;
	log_f	     log;
	void        *logger;
	int          argc;
	char const **argv;
};

int initialize(struct client_config *config, struct ducq_listen_ctx *ctx);
void finalize(void *ctx);

#endif // _DUCQ_CLIENT_
