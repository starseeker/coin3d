#ifndef COIN_EXAMPLES_INTERNAL_GLUE_H
#define COIN_EXAMPLES_INTERNAL_GLUE_H

/* Internal glue API access for examples - provides a thin wrapper around
   the internal glue headers for context creation and management */

#include <Inventor/system/gl.h>
#include <Inventor/C/basic.h>

/* Forward declarations */
struct cc_glglue;
typedef void * cc_glglue_offscreen_data;

#ifdef __cplusplus
extern "C" {
#endif

/* Context creation for offscreen rendering */
void cc_glglue_context_max_dimensions(unsigned int * width, unsigned int * height);
void * cc_glglue_context_create_offscreen(unsigned int width, unsigned int height);
SbBool cc_glglue_context_make_current(void * ctx);
void cc_glglue_context_reinstate_previous(void * ctx);
void cc_glglue_context_destruct(void * ctx);

/* External context callback support */
typedef struct cc_glglue_offscreen_cb_functions {
    cc_glglue_offscreen_data (*create_offscreen)(unsigned int width, unsigned int height);
    SbBool (*make_current)(cc_glglue_offscreen_data context);
    void (*reinstate_previous)(cc_glglue_offscreen_data context);
    void (*destruct)(cc_glglue_offscreen_data context);
} cc_glglue_offscreen_cb_functions; 

void cc_glglue_context_set_offscreen_cb_functions(cc_glglue_offscreen_cb_functions* p);

#ifdef __cplusplus
}
#endif

#endif /* COIN_EXAMPLES_INTERNAL_GLUE_H */