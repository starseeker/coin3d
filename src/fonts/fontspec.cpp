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
  Modernized to use std::string instead of cc_string.
*/

#include "coindefs.h"
#include "fonts/fontspec.h"
#include <string>
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
  
  /* Initialize strings using std::string constructors */
  new (&spec->name) std::string();
  new (&spec->style) std::string();
  
  if (name_style) {
    /* Simple parsing: assume format is "fontname" or "fontname:style" */
    const char * colon = strchr(name_style, ':');
    if (colon) {
      /* Extract font name and style */
      size_t name_len = colon - name_style;
      spec->name = std::string(name_style, name_len);
      spec->style = std::string(colon + 1);
    } else {
      /* Just font name, no style */
      spec->name = std::string(name_style);
      spec->style = std::string();
    }
  } else {
    /* Default font name and style */
    spec->name = std::string("defaultFont");
    spec->style = std::string();
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
  
  /* Copy strings using std::string copy constructor */
  new (&to->name) std::string(from->name);
  new (&to->style) std::string(from->style);
}

void
cc_fontspec_clean(cc_font_specification * spec)
{
  if (!spec) return;
  
  /* Clean up strings using std::string destructor */
  spec->name.~basic_string();
  spec->style.~basic_string();
}

#ifdef __cplusplus
}
#endif