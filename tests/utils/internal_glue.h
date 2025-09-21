#ifndef COIN_TESTS_INTERNAL_GLUE_H
#define COIN_TESTS_INTERNAL_GLUE_H

/* Internal glue API access for tests - provides a thin wrapper around
   the internal glue headers with proper include path resolution */

#define COIN_INTERNAL
#include <Inventor/system/gl.h>
#include <Inventor/C/basic.h>

/* Forward declaration of cc_libhandle */
typedef struct cc_libhandle_struct * cc_libhandle;

/* Forward declarations to avoid including complex internal headers */
struct cc_glglue;

/* Essential API functions that tests need - declarations only */
#ifdef __cplusplus
extern "C" {
#endif

/* Core context management */
const struct cc_glglue * cc_glglue_instance(int contextid);

/* Version and capability queries */
void cc_glglue_glversion(const struct cc_glglue * glue,
                         unsigned int * major,
                         unsigned int * minor,
                         unsigned int * release);

SbBool cc_glglue_glversion_matches_at_least(const struct cc_glglue * glue,
                                            unsigned int major,
                                            unsigned int minor,
                                            unsigned int release);

SbBool cc_glglue_glext_supported(const struct cc_glglue * glue, const char * extname);

/* Context creation for offscreen rendering */
void cc_glglue_context_max_dimensions(unsigned int * width, unsigned int * height);
void * cc_glglue_context_create_offscreen(unsigned int width, unsigned int height);
SbBool cc_glglue_context_make_current(void * ctx);
void cc_glglue_context_reinstate_previous(void * ctx);
void cc_glglue_context_destruct(void * ctx);

/* FBO support queries */
SbBool cc_glglue_has_framebuffer_objects(const struct cc_glglue * glue);

/* External context callback support */
typedef void * cc_glglue_offscreen_data;

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

#endif /* COIN_TESTS_INTERNAL_GLUE_H */