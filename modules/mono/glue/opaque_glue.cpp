#include "collections_glue.h"
#include "gd_glue.h"
#include "rid_glue.h"
#include "nodepath_glue.h"
#include "base_object_glue.h"
#include "string_glue.h"
/**
 * Registers internal calls that were not generated. This function is called
 * from the generated GodotSharpBindings::register_generated_icalls() function.
 */

void godot_register_glue_header_icalls() {
	godot_register_collections_icalls();
	godot_register_gd_icalls();
	godot_register_nodepath_icalls();
	godot_register_object_icalls();
	godot_register_rid_icalls();
	godot_register_string_icalls();
}
