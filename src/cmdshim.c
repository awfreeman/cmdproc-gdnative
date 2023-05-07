#include "cmdshim.h"
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#define BUFFER_DEFAULT_LEN 512


typedef enum {
	OK = 0,
	PROCESS_STILL_EXISTS,
	PROCESS_CREATION_FAILURE,
	NO_ARGS_ERROR,
	INVALID_ARGS_ERROR,
	INVALID_FILE,
	UNKNOWN_FGET_ERROR
} run_command_error;

typedef struct user_data_struct {
	FILE *proc;
	godot_char_string command;
	char *line_buffer;
	size_t buffer_length;
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

GD_METHOD(cmdshim_exec_cmd);
GD_METHOD(cmdshim_read_line);

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
	nativescript_api->godot_nativescript_register_class(p_handle, "CMDSHIM", "Reference", create, destroy);


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
	user_data->proc = NULL;
	user_data->line_buffer = api->godot_alloc(BUFFER_DEFAULT_LEN*sizeof(char));
	user_data->buffer_length = BUFFER_DEFAULT_LEN;
	return user_data;
}

// The destructor is called when Godot is done with our
// object and we free our instances' member data.
GDCALLINGCONV void simple_destructor(godot_object *p_instance, void *p_method_data, void *p_user_data) {
	user_data_struct *user_data = (user_data_struct *)p_user_data;
	if (user_data->proc){
		pclose(user_data->proc);
		api->godot_char_string_destroy(&user_data->command);
	}
	api->godot_free(user_data->line_buffer);
	api->godot_free(p_user_data);
}

GD_METHOD(cmdshim_exec_cmd){
	user_data_struct *userdata = (user_data_struct *)p_user_data;
	godot_variant ret;
	if (userdata->proc){
		api->godot_variant_new_int(&ret, PROCESS_STILL_EXISTS);
		return ret;
	}
	if (!p_num_args || p_num_args > 1 || api->godot_variant_get_type(p_args[0]) != GODOT_VARIANT_TYPE_STRING){
		api->godot_variant_new_int(&ret, INVALID_ARGS_ERROR);
		return ret;
	}
	godot_string gstring = api->godot_variant_as_string(p_args[0]);
	userdata->command = api->godot_string_utf8(&gstring);
	const char *cstrptr = api->godot_char_string_get_data(&userdata->command);
	FILE *out = popen(cstrptr, "r");
	if (!out){
		api->godot_char_string_destroy(&userdata->command);
		api->godot_variant_new_int(&ret, PROCESS_CREATION_FAILURE);
		return ret;
	}
	userdata->proc = out;
	api->godot_variant_new_int(&ret, OK);
	return ret;
}

GD_METHOD(cmdshim_read_line){
	user_data_struct *userdata = (user_data_struct *)p_user_data;
	godot_variant ret;
	if (!userdata->proc){
		api->godot_variant_new_int(&ret, (int64_t)INVALID_FILE);
		return ret;
	}
	else if(feof(userdata->proc)){
		int exit_code = pclose(userdata->proc);
		userdata->proc=NULL;
		api->godot_char_string_destroy(&userdata->command);
		godot_string str;
		api->godot_string_new(&str);
		api->godot_string_parse_utf8(&str, "Process exited");
		api->godot_variant_new_string(&ret, &str);
		api->godot_string_destroy(&str);
		return ret;
	}
	godot_string string;
	size_t offset = 0;
	size_t lend = -1;
	while(fgets(userdata->line_buffer+offset, userdata->buffer_length, userdata->proc)){
		size_t pos = 0;
		while(pos < userdata->buffer_length){
			if (userdata->line_buffer[pos] == '\n'){
				lend = pos;
				break;
			}
			else if (userdata->line_buffer[pos] == '\0'){
				break;
			}
			pos++;
		}
		if (lend == -1 && !feof(userdata->proc)){
			offset = userdata->buffer_length-1;
			userdata->line_buffer = api->godot_realloc(userdata->line_buffer, userdata->buffer_length + 128);
			userdata->buffer_length += 128;
			continue;
		}
		api->godot_string_new(&string);
		api->godot_string_parse_utf8(&string, userdata->line_buffer);
		break;
	}
	api->godot_variant_new_string(&ret, &string);
	api->godot_string_destroy(&string);
	return ret;
}