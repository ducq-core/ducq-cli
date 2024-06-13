#include <stdio.h>
#include <string.h>

#include <ducq.h>		// uses
#include <ducq_log.h>		// uses
#include "../ducq_client.h" 	// implements

static bool with_time = false;
static bool silent = false;

void print(const char *msg, size_t size, const char *color) {
	printf("%s", color);

	if(with_time) {
		char now[DUCQ_TIMESTAMP_SIZE] = "";
		ducq_getnow(now, sizeof(now));
		printf("\n%s\n", now);
	}
	printf("%.*s\n", (int)size, msg);
	printf(FG_NORMAL);
}

int on_message(ducq_i *ducq, char *msg, size_t size, void *ctx) {
	print(msg, size, FG_NORMAL);
	return 0;
}
int on_protocol(ducq_i *ducq, char *msg, size_t size, void *ctx) {
	if(! silent) {
		print(msg, size, FG_LITE_BLACK);
	}
	return 0;
}
int on_nack(ducq_i *ducq, char *msg, size_t size, void *ctx) {
	print(msg, size, FG_LITE_YELLOW);
	return -1;
}
int on_error(ducq_i *ducq, ducq_state state, void *ctx) {
	const char *msg = ducq_state_tostr(state);
	size_t size = strlen(msg);
	print(msg, size, FG_LITE_RED);
	return  -1;  
}

int initialize(struct client_config *config, struct ducq_listen_ctx *ctx){
	(void) config;

	ctx->on_message  = on_message;
	ctx->on_protocol = on_protocol;
	ctx->on_nack     = on_nack;
	ctx->on_error    = on_error;
	ctx->ctx         = NULL;

	silent = config->silent;

	const char **argv = config->argv;
	while( *(++argv) ) {
	     if(strcmp(*argv, "--time")    == 0 || strcmp(*argv, "-t") == 0) {
		     with_time = true;
	     }
	}

	return 0;
}

void finalize(void *ctx) {
}
