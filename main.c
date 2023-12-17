#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <setjmp.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h> 

#include <unistd.h> // pid(), daemon(), sleep()
#include <limits.h> // PATH_MAX

#include <ducq.h>
#include <ducq_log.h>
#include <ducq_tcp.h>

#include <lua.h>
#include <lauxlib.h>

#include "ducq_client.h"


// global: used in signal handler
jmp_buf quit;
log_f logfunc 	= NULL;
void *logger	= NULL;
#define LOGD(fmt, ...) logfunc(logger, DUCQ_LOG_DEBUG,   fmt ,##__VA_ARGS__)
#define LOGI(fmt, ...) logfunc(logger, DUCQ_LOG_INFO,    fmt ,##__VA_ARGS__)
#define LOGW(fmt, ...) logfunc(logger, DUCQ_LOG_WARNING, fmt ,##__VA_ARGS__)
#define LOGE(fmt, ...) logfunc(logger, DUCQ_LOG_ERROR,   fmt ,##__VA_ARGS__)

void error_quit(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

	exit(EXIT_FAILURE);
}


void exit_print_help() {
	error_quit(
		"ducq \n"
		"    -h,  --host       server host address (default: localhost )\n"
		"    -p,  --port       server port         (default: 9090 )\n"
		"    -c,  --command    mandatory. use 'list_commands' to get servers's available commands.\n"
		"    -r,  --route      route to publish to (default: '*')\n"
		"    -l,  --payload    payload to be sent  (default: '\\0' )\n"
		"\n\n"
	);
}



void signal_handler(int sig) {
	switch(sig) {
		case SIGTERM:
			LOGI("received SIGTERM");
			longjmp(quit, -1);
			break;
		case SIGINT :
			LOGI("received SIGINT");
			longjmp(quit, -1);
			break;
		case  SIGQUIT:
			LOGI("received SIGQUIT");
			LOGI("becoming daemon");
			if( daemon(0, 0) )
				LOGE("daemon() failed: %s\n", strerror(errno));
			else
				LOGI("became daemon", getpid());
			break;
	}
};

void set_signals() {
	if(signal(SIGTERM, signal_handler) == SIG_ERR
	|| signal(SIGINT,  signal_handler) == SIG_ERR
	|| signal(SIGQUIT, signal_handler) == SIG_ERR
	|| signal(SIGPIPE, SIG_IGN       ) == SIG_ERR) {
		error_quit("signal() failed: %s\n", strerror(errno));
	}
}

static
void log_error(const char *msg, ducq_state state) {
	LOGE("%s: %s (%s)", msg, ducq_state_tostr(state), strerror(errno));
}
ducq_state emit(ducq_i **ducqptr, struct client_config *conf, struct ducq_listen_ctx *client) {
	ducq_i *ducq     = NULL;
	ducq_state state = DUCQ_OK;

	LOGI("%s:%s", conf->host, conf->port);
	LOGI("'%s %s\n%s'", conf->command, conf->route, conf->payload);

	if(ducq) {
		ducq_close(ducq);
		ducq_free(ducq);
	}
	ducq = ducq_new_tcp(conf->host, conf->port);
	if(!ducq) {
		log_error("ducq_new_tcp() failed", state);
		longjmp(quit, -1);
	}
	*ducqptr = ducq;

	int try;
	for(try = 0; try < 3; try++) {
		LOGI("connection try #%d...", try+1);
		sleep(try * 5);

		ducq_close(ducq);
		if( state = ducq_conn(ducq) ) {
			log_error("ducq_conn() failed", state);
		}
		else if( state = ducq_timeout(ducq, 60) ) {
			log_error("ducq_timeout() failed", state);
		}
		else if( state = ducq_emit(ducq,
			conf->command, conf->route,
			conf->payload, strlen(conf->payload)
		)) {
			log_error("ducq_emit() failed", state);
		}
		else {
			LOGI("listening");
			if( state = ducq_listen(ducq, client) ) {
				log_error("ducq_listen() failed", state);
				if(state == DUCQ_ENOCMD) break;
			}
		}
	}

	LOGI("done after %d tries", try);
}



void get_config(int argc, char const *argv[], struct client_config *c) {
	if(argc > 1 && strcmp(argv[1], "--help") == 0) {
		exit_print_help();
	}

	// defaults
	c->host    = "localhost";
	c->port    = "9090";
	c->command = "list_commands";
	c->route   = "*";
	c->payload = "";

	// Lua
	lua_State *L = luaL_newstate();
	static char path[PATH_MAX] = "";
	snprintf(path, PATH_MAX, "%s/.config/ducq.lua", getenv("HOME"));
	int error = luaL_loadfile(L, path) || lua_pcall(L, 0, 0, 0);
	if(!error) {
		static char host[256] = "";
		static char port[  6] = "";
		if( lua_getglobal(L, "host") == LUA_TSTRING ) {
			strncpy(host, lua_tostring(L, -1), 256);
			host[255] = '\0';
			c->host = host;
		}
		if( lua_getglobal(L, "port") == LUA_TSTRING ) {
			strncpy(port, lua_tostring(L, -1), 256);
			port[5] = '\0';
			c->port = port;
		}
	}
	if(L) lua_close(L);

	// args
	while( *(++argv) ) {
		     if(strcmp(*argv, "--host")    == 0 || strcmp(*argv, "-h") == 0)
			c->host = *(++argv);
		else if(strcmp(*argv, "--port")    == 0 || strcmp(*argv, "-p") == 0)
			c->port = *(++argv);
		else if(strcmp(*argv, "--command") == 0 || strcmp(*argv, "-c") == 0)
			c->command = *(++argv);
		else if(strcmp(*argv, "--route")   == 0 || strcmp(*argv, "-r") == 0)
			c->route = *(++argv);
		else if(strcmp(*argv, "--payload") == 0 || strcmp(*argv, "-l") == 0)
			c->payload = *(++argv);
	}
}



int default_log(void *ctx, enum ducq_log_level level,  const char *fmt, ...) {
	FILE *file = (FILE*) ctx;

	fprintf(file, "pid %d: ", getpid());
	fprintf(file, "[%s]", ducq_level_tostr(level));

	va_list args;
	va_start(args, fmt);
	vfprintf(file, fmt, args);
	va_end(args);

	fputc('\n', file);

	return 0;
}



int main(int argc, char const *argv[]) {
	ducq_i *ducq                  = NULL;
	struct client_config   conf   = {};
	struct ducq_listen_ctx client = {};

	get_config(argc, argv, &conf);
	if( initialize(&conf, &client) )
		error_quit("client initialization failed.\n");

	logfunc = conf.log    ? conf.log    : default_log;
	logger  = conf.logger ? conf.logger : stdout;

	if( setjmp(quit) )
		goto done;
	set_signals();

	emit(&ducq, &conf, &client);

done:
	ducq_close(ducq);
	ducq_free(ducq);
	LOGI("finalizing.");
	finalize(client.ctx);
	return 0;
}
