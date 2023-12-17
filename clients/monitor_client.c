#include <stdio.h>
#include <string.h>

#include <ducq.h>		// uses
#include <ducq_log.h>		// uses
#include "../ducq_client.h" 	// implements


int on_message(ducq_i *ducq, char *msg, size_t size, void *ctx) {
	     if(strncmp("debug",  msg, 5) == 0) printf(FG_LITE_BLACK);
	else if(strncmp("info",   msg, 4) == 0) printf(FG_NORMAL);
	else if(strncmp("warn",   msg, 4) == 0) printf(FG_DARK_YELLOW);
	else if(strncmp("error",  msg, 5) == 0) printf(FG_LITE_RED);


	printf("%.*s\n", (int)size, msg);
	printf(FG_NORMAL);
	return 0;
}
int on_protocol(ducq_i *ducq, char *msg, size_t size, void *ctx) {
	printf(FG_DARK_GREEN);
	printf("%.*s\n", (int)size, msg);
	printf(FG_NORMAL);
	return 0;
}
int on_error(ducq_i *ducq, char *msg, size_t size, void *ctx) {
	printf(FG_LITE_RED);
	printf("%.*s\n", (int)size, msg);
	printf(FG_NORMAL);
	return  -1;
}

int initialize(struct client_config *config, struct ducq_listen_ctx *ctx){
	config->command = "subscribe";
	config->route   = DUCQ_LOG_ROUTE;

	ctx->on_message  = on_message;
	ctx->on_protocol = on_protocol;
	ctx->on_error    = on_error;
	ctx->ctx         = NULL;

	return 0;
}

void finalize(void *ctx) {
}
