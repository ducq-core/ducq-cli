#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <setjmp.h>
#include <stdarg.h>
#include <errno.h>

#include <ducq.h>
#include <ducq_tcp.h>


const char *prog    = NULL;
const char *host    = NULL;
const char *port    = NULL;
const char *command = NULL;
const char *route   = NULL;
const char *payload = NULL;
char payload_buffer[DUCQ_MSGSZ] = "";

ducq_i *ducq;


jmp_buf quit;

void stop_signal(int sig) {
	fprintf(stderr, "received %d\n", sig);

	if(sig == SIGINT)
		longjmp(quit, -1);
};


void set_signals() {
	if(signal(SIGINT, stop_signal) == SIG_ERR) {
		fprintf(stderr, "signal() failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	if(signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
		fprintf(stderr, "signal() failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
}


void print_help() {
	fprintf(stderr, 
		"ducq \n"
		"    -h,  --host       server host address (default: localhost )\n"
		"    -p,  --port       server port         (default: 9090 )\n"
		"    -c,  --command    mandatory. use 'list_commands' to get servers's available commands.\n"
		"    -r,  --route      route to publish to (default: '*')\n"
		"    -l,  --payload    payload to be sent  (default: '\\0' )\n"
		"                      use '-' to read from stdin.\n"
		"\n\n"
	);
	exit(EXIT_FAILURE);
}


void clean_quit() {
	ducq_close(ducq);
	ducq_free(ducq);
	printf("all done.\n");
	exit(EXIT_SUCCESS);
}

void error_quit(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

	exit(EXIT_FAILURE);
}


void emit() {
	ducq_state state = DUCQ_OK;

	ducq = ducq_new_tcp(host, port);
	if(!ducq) error_quit("ducq_new_tcp() failed\n");

	state = ducq_conn(ducq);
	if(state) error_quit("ducq_conn() failed: %s\n", ducq_state_tostr(state));

	state = ducq_emit(ducq, command, route, payload, strlen(payload), false);
	if(state) error_quit("ducq_emit() failed: %s\n", ducq_state_tostr(state));

//	if(strcmp(command, "subscribe") != 0) {
//		state = ducq_timeout(ducq, 5);
//		if(state) error_quit("ducq_timeout() failed: %s\n", ducq_state_tostr(state));
//	}
}

void parse_args(int argc, char const *argv[]) {
	while( *(++argv) ) {
		printf("argv: %s\n", *argv);
		     if(strcmp(*argv, "--host")    == 0 || strcmp(*argv, "-h") == 0)
			host = *(++argv);
		else if(strcmp(*argv, "--port")    == 0 || strcmp(*argv, "-p") == 0)
			port = *(++argv);
		else if(strcmp(*argv, "--command") == 0 || strcmp(*argv, "-c") == 0)
			command = *(++argv);
		else if(strcmp(*argv, "--route")   == 0 || strcmp(*argv, "-r") == 0)
			route = *(++argv);
		else if(strcmp(*argv, "--payload") == 0 || strcmp(*argv, "-l") == 0)
			payload = *(++argv);
	}

	if(!host)    host = getenv("DUCQ_HOST");
	if(!port)    port = getenv("DUCQ_PORT");
	if(!host)    host = "localhost";
	if(!port)    port = "9090";
	if(!command) print_help();
	if(!route)   route   = "*";
	if(!payload) payload = "";

	if(*payload == '-') {
		size_t size = DUCQ_MSGSZ - strlen(command) - strlen(route) - 4;  //' ' + '\n' + '\0' + receiver's '\0'
		fread(payload_buffer, sizeof(char), size, stdin);
		payload_buffer[DUCQ_MSGSZ] = '\0';
		payload = payload_buffer;
	}

	printf("%s:%s\n'%s %s'\n%s\n\n",
		host, port, command, route, payload
	);

}




int main(int argc, char const *argv[])
{
	if(argc == 1 || strcmp(argv[1], "--help") == 0) {
		print_help();
		exit(EXIT_FAILURE);
	}

	set_signals();
	if( setjmp(quit) )
		clean_quit();

	parse_args(argc, argv);
	emit();

	
	printf("---\n\n");

	for(;;) {
		ducq_state state     = DUCQ_OK;
		char msg[DUCQ_MSGSZ] = "";
		size_t len           = DUCQ_MSGSZ;

		state = ducq_recv(ducq, msg, &len);
		if(state != DUCQ_OK) {
			printf("end: %s\n", ducq_state_tostr(state));
			break;
		}
		printf("%.*s\n", (int)len, msg);
	}

	printf("---\n");
	clean_quit();
}
