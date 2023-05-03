#ifndef CMDSHIM_H
#define CMDSHIM_H

#include <gdnative_api_struct.gen.h>
#include <string.h>
#include <stdlib.h>
#define GD_METHOD(fname) godot_variant fname(godot_object *p_instance, void *p_method_data, void *p_user_data, int p_num_args, godot_variant **p_args)

#define INIT_GD_METHOD(fname) \
    godot_instance_method fname = { NULL, NULL, NULL }; \
    fname.method = &cmdshim_##fname; \
    nativescript_api->godot_nativescript_register_method(p_handle, "CMDSHIM", #fname, attributes, fname)

#endif //CMDPROC_H
