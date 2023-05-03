#include "cmdproc.h"
#include <unistd.h>
#include <errno.h>
#include <spawn.h>
#include <sys/socket.h>
#include <pipeline.h>
#include <fcntl.h>

typedef struct user_data_struct {
	pipeline *cmdpipe;
} user_data_struct;



// GDNative supports a large collection of functions for calling back
// into the main Godot executable. In order for your module to have
// access to these functions, GDNative provides your application with
// a struct containing pointers to all these functions.
const godot_gdnative_core_api_struct *api = NULL;
const godot_gdnative_ext_nativescript_api_struct *nativescript_api = NULL;


// These are forward declarations for the functions we'll be implementing
// for our object. A constructor and destructor are both necessary.
GDCALLINGCONV void *simple_constructor(godot_object *p_instance, void *p_method_data);
GDCALLINGCONV void simple_destructor(godot_object *p_instance, void *p_method_data, void *p_user_data);
//godot_variant simple_get_data(godot_object *p_instance, void *p_method_data, void *p_user_data, int p_num_args, godot_variant **p_args);

GD_METHOD(cmdproc_exec_cmd);
GD_METHOD(cmdproc_read_line);

// `gdnative_init` is a function that initializes our dynamic library.
// Godot will give it a pointer to a structure that contains various bits of
// information we may find useful among which the pointers to our API structures.
void GDN_EXPORT godot_gdnative_init(godot_gdnative_init_options *p_options) {
	api = p_options->api_struct;

	// Find NativeScript extensions.
	for (int i = 0; i < api->num_extensions; i++) {
		switch (api->extensions[i]->type) {
			case GDNATIVE_EXT_NATIVESCRIPT: {
				nativescript_api = (godot_gdnative_ext_nativescript_api_struct *)api->extensions[i];
			}; break;
			default:
				break;
		};
	};
}

// `gdnative_terminate` which is called before the library is unloaded.
// Godot will unload the library when no object uses it anymore.
void GDN_EXPORT godot_gdnative_terminate(godot_gdnative_terminate_options *p_options) {
	api = NULL;
	nativescript_api = NULL;
}

// `nativescript_init` is the most important function. Godot calls
// this function as part of loading a GDNative library and communicates
// back to the engine what objects we make available.
void GDN_EXPORT godot_nativescript_init(void *p_handle) {
	godot_instance_create_func create = { NULL, NULL, NULL };
	create.create_func = &simple_constructor;

	godot_instance_destroy_func destroy = { NULL, NULL, NULL };
	destroy.destroy_func = &simple_destructor;

	// We first tell the engine which classes are implemented by calling this.
	// * The first parameter here is the handle pointer given to us.
	// * The second is the name of our object class.
	// * The third is the type of object in Godot that we 'inherit' from;
	//   this is not true inheritance but it's close enough.
	// * Finally, the fourth and fifth parameters are descriptions
	//   for our constructor and destructor, respectively.
	nativescript_api->godot_nativescript_register_class(p_handle, "CMDPROC", "Reference", create, destroy);


	godot_method_attributes attributes = { GODOT_METHOD_RPC_MODE_DISABLED };

	INIT_GD_METHOD(exec_cmd);
	INIT_GD_METHOD(read_line);
}

// In our constructor, allocate memory for our structure and fill
// it with some data. Note that we use Godot's memory functions
// so the memory gets tracked and then return the pointer to
// our new structure. This pointer will act as our instance
// identifier in case multiple objects are instantiated.
GDCALLINGCONV void *simple_constructor(godot_object *p_instance, void *p_method_data) {
	user_data_struct *user_data = api->godot_alloc(sizeof(user_data_struct));
	user_data->cmdpipe = NULL;
	return user_data;
}

// The destructor is called when Godot is done with our
// object and we free our instances' member data.
GDCALLINGCONV void simple_destructor(godot_object *p_instance, void *p_method_data, void *p_user_data) {
	user_data_struct *user_data = (user_data_struct *)p_user_data;
	if (user_data->cmdpipe){
		pipeline_free(user_data->cmdpipe);
	}
	api->godot_free(p_user_data);
}

typedef enum {
	OK = 0,
	PIPE_ALREADY_EXISTS,
	NO_ARGS_ERROR,
	INVALID_ARGS_ERROR,
} run_command_error;

typedef struct gstringlist{
	godot_string *gstrings;
	godot_char_string *gcharstrings;
	const char **cstrings;
	int len;
} gstringlist;

int newgstringlist(gstringlist *list, godot_variant **variants, int len){
	*list = (gstringlist){
		api->godot_alloc(sizeof(godot_string)*len),
		api->godot_alloc(sizeof(godot_char_string)*len),
		api->godot_alloc(sizeof(__SIZEOF_POINTER__)*len),
		len
	};
	for (int i = 0; i < len; i++){
		if(api->godot_variant_get_type(variants[i]) != GODOT_VARIANT_TYPE_STRING){
			//we only destroy those that allocate (see https://github.com/godotengine/godot/blob/3.5/modules/gdnative/gdnative/string.cpp)
			for (int a = 0; a < i-1; i++){
				api->godot_char_string_destroy(&list->gcharstrings[i]);
				api->godot_string_destroy(&list->gstrings[i]);
			}
			api->godot_free(list->cstrings);
			api->godot_free(list->gcharstrings);
			api->godot_free(list->gstrings);
			return -1;
		}
		list->gstrings[i] = api->godot_variant_as_string(variants[i]);
		//api->godot_string_strip_edges(&list->gstrings[i], true, true);
		list->gcharstrings[i] = api->godot_string_utf8(&list->gstrings[i]);
		list->cstrings[i] = api->godot_char_string_get_data(&list->gcharstrings[i]);
	}
	return 0;
}

void destroygstringlist(gstringlist *list){
	for (int i = 0; i < list->len; i++){
		api->godot_char_string_destroy(&list->gcharstrings[i]);
		api->godot_string_destroy(&list->gstrings[i]);
	}
	api->godot_free(list->cstrings);
	api->godot_free(list->gcharstrings);
	api->godot_free(list->gstrings);
}

GD_METHOD(cmdproc_exec_cmd){
	user_data_struct *userdata = (user_data_struct *)p_user_data;
	godot_variant ret;
	if (userdata->cmdpipe){
		api->godot_variant_new_int(&ret, PIPE_ALREADY_EXISTS);
		return ret;
	}
	if (!p_num_args){
		api->godot_variant_new_int(&ret, NO_ARGS_ERROR);
		return ret;
	}
	gstringlist strlist;
	if(newgstringlist(&strlist, p_args, p_num_args)){
		api->godot_variant_new_int(&ret, INVALID_ARGS_ERROR);
		return ret;
	}

	pipeline *cmdpipe = pipeline_new();
	/*pipecmd *cmd = pipecmd_new("ping");
	pipecmd_arg(cmd, "-c4");
	pipecmd_arg(cmd, "8.8.8.8");
	pipeline_command(cmdpipe, cmd);*/
	pipecmd *cmd = pipecmd_new(strlist.cstrings[0]);
	for (int i = 1; i < strlist.len; i++){
		pipecmd_arg(cmd, strlist.cstrings[i]);
	}
	pipeline_command(cmdpipe, cmd);
	pipeline_want_out(cmdpipe, -1);
	pipeline_start(cmdpipe);
	destroygstringlist(&strlist);
	userdata->cmdpipe = cmdpipe;
	api->godot_variant_new_nil(&ret);
	return ret;
}

GD_METHOD(cmdproc_read_line){
	user_data_struct *userdata = (user_data_struct *)p_user_data;
	godot_variant ret;
	if (!userdata->cmdpipe){
		api->godot_variant_new_nil(&ret);
		return ret;
	}
	const char* line = pipeline_readline(userdata->cmdpipe);
	if (!line){
		int x = pipeline_wait(userdata->cmdpipe);
		pipeline_free(userdata->cmdpipe);
		api->godot_variant_new_int(&ret, x);
		userdata->cmdpipe = NULL;
		return ret;
	}
	godot_string str;
	api->godot_string_new(&str);
	api->godot_string_parse_utf8(&str, line);
	api->godot_variant_new_string(&ret, &str);
	return ret;
}