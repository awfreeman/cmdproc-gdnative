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
	char *line_buffer;
	size_t buffer_length;
	run_command_error errcode;
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
	}
	api->godot_free(user_data->line_buffer);
	api->godot_free(p_user_data);
}

GD_METHOD(cmdshim_exec_cmd){
	user_data_struct *userdata = (user_data_struct *)p_user_data;
	godot_variant ret;
	if (userdata->proc){
		userdata->errcode = PROCESS_STILL_EXISTS;
		api->godot_variant_new_nil(&ret);
		return ret;
	}
	if (!p_num_args || p_num_args > 1 || api->godot_variant_get_type(p_args[0]) != GODOT_VARIANT_TYPE_STRING){
		userdata->errcode = INVALID_ARGS_ERROR;
		api->godot_variant_new_nil(&ret);
		return ret;
	}
	const godot_string gstring = api->godot_variant_as_string(p_args[0]);
	const godot_char_string gcstring = api->godot_string_utf8(&gstring);
	const char *cstrptr = api->godot_char_string_get_data(&gcstring);
	FILE *out = popen(cstrptr, "r");
	if (!out){
		userdata->errcode = PROCESS_CREATION_FAILURE;
		api->godot_variant_new_nil(&ret);
		return ret;
	}
	userdata->proc = out;		
	userdata->errcode = OK;
	api->godot_variant_new_nil(&ret);
	return ret;
}

int readline(user_data_struct *self, godot_string *string){
	size_t offset = 0;
	size_t lend = -1;
	while(fgets(self->line_buffer+offset, self->buffer_length, self->proc)){
		for(size_t x = offset; x<self->buffer_length; x++){
			if (self->line_buffer[x] == '\n'){
				lend = x;
				break;
			}
		}
		if (lend == -1){
			offset = self->buffer_length-1;
			self->line_buffer = api->godot_realloc(self->line_buffer, self->buffer_length + 128);
			self->buffer_length+= 128;
			continue;
		}
		else {
			api->godot_string_new(string);
			api->godot_string_parse_utf8(string, self->line_buffer);
			return 0;
		}
	}
	if(feof(self->proc)){
		pclose(self->proc);
		api->godot_string_new(string);
		api->godot_string_parse_utf8(string, self->line_buffer);

		return 0;
	}
	return -1;
}

GD_METHOD(cmdshim_read_line){
	user_data_struct *userdata = (user_data_struct *)p_user_data;
	godot_variant ret;
	if (!userdata->proc){
		api->godot_variant_new_nil(&ret);
		userdata->errcode = INVALID_FILE;
		return ret;
	}
	godot_string str;
	if (readline(userdata,  &str) == -1){
		api->godot_variant_new_nil(&ret);
		userdata->errcode = UNKNOWN_FGET_ERROR;
		return ret;
	}
	api->godot_variant_new_string(&ret, &str);
	return ret;
}