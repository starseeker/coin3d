/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
\**************************************************************************/

/*
  Basic implementations for font specification handling.
*/

#include "coindefs.h"
#include "fonts/fontspec.h"
#include <Inventor/C/base/string.h>
#include <cstring>
#include <cstdlib>

#ifdef __cplusplus
extern "C" {
#endif

void
cc_fontspec_construct(cc_font_specification * spec,
                      const char * name_style,
                      float size, float complexity)
{
  if (!spec) return;
  
  /* Initialize the font specification with default values */
  spec->size = size;
  spec->complexity = complexity;
  
  /* Initialize strings */
  cc_string_construct(&spec->name);
  cc_string_construct(&spec->style);
  
  if (name_style) {
    /* Simple parsing: assume format is "fontname" or "fontname:style" */
    const char * colon = strchr(name_style, ':');
    if (colon) {
      /* Extract font name and style */
      size_t name_len = colon - name_style;
      char * name_part = (char*)malloc(name_len + 1);
      if (name_part) {
        strncpy(name_part, name_style, name_len);
        name_part[name_len] = '\0';
        cc_string_set_text(&spec->name, name_part);
        free(name_part);
      }
      cc_string_set_text(&spec->style, colon + 1);
    } else {
      /* Just font name, no style */
      cc_string_set_text(&spec->name, name_style);
      cc_string_set_text(&spec->style, "");
    }
  } else {
    /* Default font name and style */
    cc_string_set_text(&spec->name, "defaultFont");
    cc_string_set_text(&spec->style, "");
  }
}

void
cc_fontspec_copy(const cc_font_specification * from,
                 cc_font_specification * to)
{
  if (!from || !to) return;
  
  /* Copy basic values */
  to->size = from->size;
  to->complexity = from->complexity;
  
  /* Copy strings */
  cc_string_construct(&to->name);
  cc_string_construct(&to->style);
  cc_string_set_string(&to->name, &from->name);
  cc_string_set_string(&to->style, &from->style);
}

void
cc_fontspec_clean(cc_font_specification * spec)
{
  if (!spec) return;
  
  /* Clean up strings */
  cc_string_clean(&spec->name);
  cc_string_clean(&spec->style);
}

#ifdef __cplusplus
}
#endif