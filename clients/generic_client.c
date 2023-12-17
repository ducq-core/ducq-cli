#include <stdio.h>
#include <string.h>

#include <ducq.h>		// uses
#include <ducq_log.h>		// uses
#include "../ducq_client.h" 	// implements

static bool with_time = false;

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
	print(msg, size, FG_LITE_BLACK);
	return 0;
}
int on_error(ducq_i *ducq, char *msg, size_t size, void *ctx) {
	print(msg, size, FG_LITE_RED);
	return  -1;  
}

int initialize(struct client_config *config, struct ducq_listen_ctx *ctx){
	(void) config;

	ctx->on_message  = on_message;
	ctx->on_protocol = on_protocol;
	ctx->on_error    = on_error;
	ctx->ctx         = NULL;

	const char **argv = config->argv;
	while( *(++argv) ) {
	     if(strcmp(*argv, "--time")    == 0 || strcmp(*argv, "-t") == 0) {
		     with_time = true;
		     break;
	     }
	}

	return 0;
}

void finalize(void *ctx) {
}
