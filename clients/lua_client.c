#include <stdio.h>
#include <string.h>

#include <ducq.h>
#include <ducq_lua.h>
#include "../ducq_client.h" 	// implements

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

static log_f logfunc = NULL;
static void *logger  = NULL;
#define LOGD(fmt, ...) logfunc(logger, DUCQ_LOG_DEBUG,   fmt ,##__VA_ARGS__)
#define LOGI(fmt, ...) logfunc(logger, DUCQ_LOG_INFO,    fmt ,##__VA_ARGS__)
#define LOGW(fmt, ...) logfunc(logger, DUCQ_LOG_WARNING, fmt ,##__VA_ARGS__)
#define LOGE(fmt, ...) logfunc(logger, DUCQ_LOG_ERROR,   fmt ,##__VA_ARGS__)


static
bool is_func(lua_State *L, const char *fname) {
	return lua_getglobal(L, fname) == LUA_TFUNCTION;
}
static
int callFunction(lua_State *L, const char *fname, ducq_i *ducq, char *msg, size_t size) {
	int ret = 0;

	struct ducq_msg msg_struct = ducq_parse_msg(msg);

	lua_getglobal(L, fname);
	ducq_push_ducq(L, ducq);
	ducq_push_msg(L, &msg_struct);
	
	if( lua_pcall(L, 2, 1, 0) != LUA_OK ) {
		LOGE("lua_pcall() failed for %s: %s\n",
			fname, lua_tostring(L, -1));
		ret =  -1;
	}
	else if( ! lua_isinteger(L, -1) ) {
		LOGE("%s() did not return a integer.\n", fname);
		ret = -1;
	}
	else {
		ret = lua_tointeger(L, -1);
	}

	lua_pop(L, 1);
	return ret;
}
static
int on_message(ducq_i *ducq, char *msg, size_t size, void *ctx) {
	lua_State *L = (lua_State *)ctx;
	return callFunction(L, "onMessage", ducq, msg, size);	
}
static
int on_protocol(ducq_i *ducq, char *msg, size_t size, void *ctx) {
	lua_State *L = (lua_State *)ctx;
	return callFunction(L, "onProtocol", ducq, msg, size);	
}
static
int on_nack(ducq_i *ducq, char *msg, size_t size, void *ctx) {
	lua_State *L = (lua_State *)ctx;
	return callFunction(L, "onNack", ducq, msg, size);
}
static
int on_error(ducq_i *ducq, ducq_state state, void *ctx) {
	lua_State *L = (lua_State *)ctx;
	int ret = 0;

	const char *fname = "onError";

	lua_getglobal(L, fname);
	ducq_push_ducq(L, ducq);
	lua_pushnumber(L, state);

	if( lua_pcall(L, 2, 1, 0) != LUA_OK ) {
		LOGE("lua_pcall() failed for %s: %s\n",
			fname, lua_tostring(L, -1));
		ret =  -1;
	}
	else if( ! lua_isinteger(L, -1) ) {
		LOGE("%s() did not return a integer.\n", fname);
		ret = -1;
	}
	else {
		ret = lua_tointeger(L, -1);
	}

	lua_pop(L, 1);
	return ret;
}
static
int lualog(void *ctx, enum ducq_log_level level, const char *fmt, ...) {
	lua_State *L = (lua_State *)ctx;
	va_list ap;
	va_start(ap, fmt);

	lua_getglobal(L, "log");
	lua_pushstring(L, ducq_level_tostr(level));
	lua_pushvfstring(L, fmt, ap);

	va_end(ap);
	
	if( lua_pcall(L, 2, 0, 0) != LUA_OK ) {
		fprintf(stderr, "lua_pcall() failed for log: %s\n", lua_tostring(L, -1));
	}
}



char _host[256];
char _port[256];
char _command[256];
char _route[256];
char _payload[256];

static
void load_string(lua_State *L, const char *str, char *holder, const char **out) {
	if( lua_getglobal(L, str) == LUA_TSTRING) {
		snprintf( holder, 256, "%s", lua_tostring(L, -1));
		*out =  holder;
	}
	lua_pop(L, 1);
}

int initialize(struct client_config *config, struct ducq_listen_ctx *ctx){
	logfunc = config->log;
	logger  = config->logger;

	lua_State *L = luaL_newstate();
	if(!L) {
		LOGE("could not allocate lua state.\n");
		return -1;
	}
	luaL_openlibs(L);
	lua_pushboolean(L, config->silent);
	lua_setglobal(L, "silent");

	const char *file = NULL;
	const char **argv = config->argv;
	while( *(++argv) ) {
	     if(strcmp(*argv, "--file")    == 0 || strcmp(*argv, "-f") == 0) {
		     file = *(++argv);
	     }
	}
	if(!file) {
		LOGE("no lua file. use '-f' or '--file'\n");
		return -1;
	}
	if( luaL_dofile(L, file) ) {
		LOGE("luaL_dofile() failed: %s\n", lua_tostring(L, -1));
		return -1;
	}


	ctx->ctx = L;
	if(is_func(L, "onMessage" )) ctx->on_message  = on_message;
	if(is_func(L, "onProtocol")) ctx->on_protocol = on_protocol;
	if(is_func(L, "onNack"    )) ctx->on_nack     = on_nack;
	if(is_func(L, "onError"   )) ctx->on_error    = on_error;
	if(is_func(L, "log"       )) {
		config->log    = lualog;
		config->logger = L;
		logfunc        = lualog;
		logger         = L;
	}

	
	load_string(L, "host",    _host,    &config->host);
	load_string(L, "port",    _port,    &config->port);
	load_string(L, "command", _command, &config->command);
	load_string(L, "route",   _route,   &config->route);
	load_string(L, "payload", _payload, &config->payload);

	return 0;
}


static
void call_finalize(lua_State *L) {
	if(( lua_getglobal(L, "finalize") != LUA_TFUNCTION)) return;
	if(( lua_pcall(L, 0, 0, 0)        == LUA_OK       )) return;
	
	LOGE("lua_pcall() failed finalize: %s\n", lua_tostring(L, -1));
}
void finalize(void *ctx) {
	lua_State *L = (lua_State *)ctx;
	call_finalize(L);
	lua_close(L);
}

