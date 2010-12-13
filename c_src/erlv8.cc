#include "v8.h"
#include "erl_nif.h"

#include <iostream>
#include <cstring>

using namespace std;
using namespace __gnu_cxx;

static v8::Persistent<v8::ObjectTemplate> global_template;
static ErlNifResourceType * script_resource;

v8::Handle<v8::Object> term_to_js(ErlNifEnv *env, ERL_NIF_TERM term); // fwd

class ErlScript {
public:
  ErlNifEnv *caller_env;
  const char *buf;
  unsigned len;

  v8::Persistent<v8::Context> context;

  ErlNifPid *server;
  ErlNifEnv *env;

  ErlScript(ErlNifEnv * a_env, const char *a_buf, unsigned a_len) : caller_env(env), buf(a_buf), len(a_len) {
	env = enif_alloc_env();
	{
	  v8::Locker locker;
	  v8::HandleScope handle_scope;
	  context = v8::Context::New(NULL, global_template);
	  v8::Context::Scope context_scope(context);
	  context->Global()->SetHiddenValue(v8::String::New("__erlv8__"),v8::External::New(this));
	}
  };

  ~ErlScript() { 
	context.Dispose();
	enif_free_env(env);
  };

  void send(ERL_NIF_TERM term) {
	enif_send(NULL, server, env, term);
	enif_clear_env(env);
  };

  void register_module(char * name, ERL_NIF_TERM exports) {
	{
	  v8::Locker locker;
	  v8::HandleScope handle_scope;
	  v8::Context::Scope context_scope(context);

	  v8::Local<v8::Object> obj = v8::Local<v8::Object>::New(term_to_js(env,exports));
	  context->Global()->Set(v8::String::New(name),obj);
	}
  };

  void run() {
	{
	  v8::Locker locker;
	  v8::HandleScope handle_scope;
	  
	  v8::Context::Scope context_scope(context);
	  
	  v8::TryCatch try_catch;
	  
	  v8::Handle<v8::String> script = v8::String::New(buf, len);
	  v8::Handle<v8::Script> compiled = v8::Script::Compile(script);
	
	if (compiled.IsEmpty()) {
	  send(enif_make_atom(env,"compilation_failed"));
	} else {
	  send(enif_make_atom(env,"starting"));
	  v8::Handle<v8::Value> value = compiled->Run();
      send(enif_make_atom(env,"finished"));
	}
    } 

};

};

typedef struct _script_res_t { 
  ErlScript * script;
} script_res_t;



static ERL_NIF_TERM new_script(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
  ERL_NIF_TERM term;

  unsigned len;
  enif_get_list_length(env, argv[0], &len);
  char * script = (char *) malloc(len + 1);
  enif_get_string(env,argv[0],script,len + 1, ERL_NIF_LATIN1);

  ErlScript *escript = new ErlScript(env, reinterpret_cast<const char *>(script),len);

  script_res_t *ptr = (script_res_t *)enif_alloc_resource(script_resource, sizeof(script_res_t));
  ptr->script = escript;

  term = enif_make_resource(env, ptr);

  enif_release_resource(ptr);

  return term;
};

static ERL_NIF_TERM get_script(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[]) 
{
  script_res_t *res;
  if (enif_get_resource(env,argv[0],script_resource,(void **)(&res))) {
     return enif_make_string(env,res->script->buf, ERL_NIF_LATIN1);
  };
  return enif_make_badarg(env);
};


void * run_script(void *data) {
  ErlScript *escript = reinterpret_cast<ErlScript *>(data);
  escript->run();
  return NULL;
};

static ERL_NIF_TERM run(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[]) {
  script_res_t *res;
  ErlNifTid tid;
  if (!enif_is_pid(env,argv[1])) {
	return enif_make_badarg(env);
  }
  if (enif_get_resource(env,argv[0],script_resource,(void **)(&res))) {
	res->script->server = (ErlNifPid *) malloc(sizeof(ErlNifPid));
	enif_get_local_pid(env, argv[1], res->script->server);
	enif_thread_create((char *)"erlv8", &tid, run_script, res->script, NULL);
  } else {
	return enif_make_badarg(env);
  };
  return enif_make_atom(env,"ok");
};

static ERL_NIF_TERM register_module(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[]) {
  script_res_t *res;
  if (enif_get_resource(env,argv[0],script_resource,(void **)(&res))) {
	unsigned len;
	enif_get_atom_length(env, argv[1], &len, ERL_NIF_LATIN1);
	char * name = (char *) malloc(len + 1);
	enif_get_atom(env,argv[1],name,len + 1, ERL_NIF_LATIN1);
	res->script->register_module(name,enif_make_copy(res->script->env,argv[2]));
  } else {
	return enif_make_badarg(env);
  };
  return enif_make_atom(env,"ok");
};

static ERL_NIF_TERM script_send(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[]) {
  script_res_t *res;
  if (enif_get_resource(env,argv[0],script_resource,(void **)(&res))) {
	res->script->send(enif_make_copy(res->script->env,argv[1]));
	return argv[1];
  } else {
	return enif_make_badarg(env);
  };
 
}

static ErlNifFunc nif_funcs[] =
{
  {"new_script", 1, new_script},
  {"get_script", 1, get_script},
  {"run", 2, run},
  {"register", 3, register_module},
  {"script_send", 2, script_send}
};

#define __ERLV8__(O) v8::Local<v8::External>::Cast(O->GetHiddenValue(v8::String::New("__erlv8__")))->Value()

ERL_NIF_TERM js_to_term(ErlNifEnv *env, v8::Handle<v8::Value> val); // fwd
v8::Handle<v8::Value> WrapFun(const v8::Arguments &arguments) {
  v8::HandleScope handle_scope;
  ErlScript * script = (ErlScript *)__ERLV8__(v8::Context::GetCurrent()->Global());

  ERL_NIF_TERM term = (ERL_NIF_TERM) arguments.Data()->ToInteger()->Value();

  script_res_t *ptr = (script_res_t *)enif_alloc_resource(script_resource, sizeof(script_res_t));
  ptr->script = script;

  ERL_NIF_TERM * resource_term_ref = (ERL_NIF_TERM *) malloc(sizeof(ERL_NIF_TERM));
  *resource_term_ref = enif_make_resource(script->env, ptr);

  v8::Local<v8::Array> array = v8::Array::New(arguments.Length() + 1);

  array->Set(0, v8::External::New(resource_term_ref));

  for (signed int i=0;i<arguments.Length();i++) {
      array->Set(i + 1,arguments[i]);
  }
  script->send(enif_make_tuple2(script->env,enif_make_copy(script->env,term),js_to_term(script->env,array)));
};

v8::Handle<v8::Object> term_to_js(ErlNifEnv *env, ERL_NIF_TERM term) {
  v8::HandleScope handle_scope;
  if (enif_is_empty_list(env,term)) {
	return v8::Object::New();
  } else if (enif_is_list(env,term)) {
    // TODO
  } else if (enif_is_fun(env, term)) {
    v8::Local<v8::FunctionTemplate> t = v8::FunctionTemplate::New(WrapFun,v8::Integer::New(term));
	v8::Local<v8::Function> f = v8::Local<v8::Function>::Cast(t->GetFunction());
	return f;
  }
  return v8::Object::New(); // if nothing else works, may be an empty object will be ok
};


ERL_NIF_TERM js_to_term(ErlNifEnv *env, v8::Handle<v8::Value> val) {
  v8::HandleScope handle_scope;
  if (val->IsUndefined()) {
    return enif_make_atom(env,"undefined");
  } else if (val->IsNull()) {
	return enif_make_atom(env,"null");
  } else if (val->IsString()) {
    return enif_make_string(env,*v8::String::AsciiValue(val->ToString()),ERL_NIF_LATIN1);
  } else if (val->IsInt32()) {
	return enif_make_long(env,val->ToInt32()->Value());
  } else if (val->IsArray()) {
    v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(val);
	ERL_NIF_TERM *arr = (ERL_NIF_TERM *) malloc(sizeof(ERL_NIF_TERM) * array->Length());
	for (unsigned int i=0;i<array->Length();i++) {
        arr[i] = js_to_term(env,array->Get(v8::Integer::NewFromUnsigned(i)));
	}
	ERL_NIF_TERM list = enif_make_list_from_array(env,arr,array->Length());
	free(arr);
	return list;
  } else if (val->IsObject()) {
    v8::Handle<v8::Object> obj = v8::Handle<v8::Object>::Cast(val);
	v8::Handle<v8::Array> keys = obj->GetPropertyNames();
	ERL_NIF_TERM *arr = (ERL_NIF_TERM *) malloc(sizeof(ERL_NIF_TERM) * keys->Length());
	for (unsigned int i=0;i<keys->Length();i++) {
	  v8::Handle<v8::Value> key = keys->Get(v8::Integer::New(i));

	  arr[i] = enif_make_tuple(env,2,
							   js_to_term(env,v8::Handle<v8::String>::Cast(key)),
							   js_to_term(env,obj->Get(key)));
	}
	ERL_NIF_TERM list = enif_make_list_from_array(env,arr,keys->Length());
	free(arr);
	return list;
  } else if (val->IsExternal()) { // passing terms
	ERL_NIF_TERM *term_ref = (ERL_NIF_TERM *) v8::External::Unwrap(val);
	ERL_NIF_TERM term = *term_ref;
	free(term_ref);
	return term;
  }
  return enif_make_badarg(env); // may this cause problems?
};


static void script_resource_destroy(ErlNifEnv* env, void* obj)
{
}

int load(ErlNifEnv *env, void** priv_data, ERL_NIF_TERM load_info)
{
  script_resource = enif_open_resource_type(env, NULL, "erlv8_resource", script_resource_destroy, (ErlNifResourceFlags) (ERL_NIF_RT_CREATE | ERL_NIF_RT_TAKEOVER), NULL);
  v8::V8::Initialize();
  v8::HandleScope handle_scope;

  global_template = v8::Persistent<v8::ObjectTemplate>::New(v8::ObjectTemplate::New());

  v8::Locker::StartPreemption(100);

  return 0;
};

void unload(ErlNifEnv *env, void* priv_data)
{
  v8::Locker::StopPreemption();
  global_template.Dispose();
};

ERL_NIF_INIT(erlv8_nif,nif_funcs,load,NULL,NULL,unload)