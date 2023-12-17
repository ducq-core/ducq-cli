#include <stdio.h>

#include <ducq.h>		// uses
#include <ducq_log.h>		// uses
#include "../ducq_client.h" 	// implements


int on_message(ducq_i *ducq, char *msg, size_t size, void *ctx) {
	printf("%.*s\n", (int)size, msg);
	return 0;
}
int on_protocol(ducq_i *ducq, char *msg, size_t size, void *ctx) {
	printf(FG_LITE_BLACK);
	printf("\033[90m%.*s\033[39m\n", (int)size, msg);
	printf(FG_NORMAL);
	return 0;
}
int on_error(ducq_i *ducq, char *msg, size_t size, void *ctx) {
	printf(FG_LITE_RED);
	printf("\033[91m%.*s\033[39m\n", (int)size, msg);
	printf(FG_NORMAL);
	return  -1;  
}

int initialize(struct client_config *config, struct ducq_listen_ctx *ctx){
	(void) config;

	ctx->on_message  = on_message;
	ctx->on_protocol = on_protocol;
	ctx->on_error    = on_error;
	ctx->ctx         = NULL;

	return 0;
}

void finalize(void *ctx) {
}
