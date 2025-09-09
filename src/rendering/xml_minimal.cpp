/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 * 
 * Minimal XML implementations to resolve linking errors when XML support is not available
 **************************************************************************/

#include <Inventor/C/XML/document.h>
#include <Inventor/C/XML/element.h>
#include <cstdlib>

// Minimal implementations for cc_xml_* functions
// These provide the same behavior as the previous cc_xml_stubs.cpp file
// but are now directly integrated where they're used

cc_xml_doc * cc_xml_doc_new(void) {
  return nullptr;
}

void cc_xml_doc_delete_x(cc_xml_doc * doc) {
  // Stub implementation - do nothing
  (void)doc; // Suppress unused parameter warning
}

SbBool cc_xml_doc_read_file_x(cc_xml_doc * doc, const char * path) {
  (void)doc; (void)path; // Suppress unused parameter warnings
  return FALSE;
}

SbBool cc_xml_doc_read_buffer_x(cc_xml_doc * doc, const char * buffer, size_t buflen) {
  (void)doc; (void)buffer; (void)buflen; // Suppress unused parameter warnings
  return FALSE;
}

cc_xml_elt * cc_xml_doc_get_root(const cc_xml_doc * doc) {
  (void)doc; // Suppress unused parameter warning
  return nullptr;
}

void cc_xml_doc_set_root_x(cc_xml_doc * doc, cc_xml_elt * root) {
  (void)doc; (void)root; // Suppress unused parameter warnings
}

cc_xml_elt * cc_xml_elt_new(void) {
  return nullptr;
}

cc_xml_elt * cc_xml_elt_new_from_data(const char * type, cc_xml_attr ** attrs) {
  (void)type; (void)attrs; // Suppress unused parameter warnings
  return nullptr;
}

cc_xml_elt * cc_xml_elt_clone(const cc_xml_elt * elt) {
  (void)elt; // Suppress unused parameter warning
  return nullptr;
}

void cc_xml_elt_set_type_x(cc_xml_elt * elt, const char * type) {
  (void)elt; (void)type; // Suppress unused parameter warnings
}

const char * cc_xml_elt_get_type(const cc_xml_elt * elt) {
  (void)elt; // Suppress unused parameter warning
  return nullptr;
}

void cc_xml_elt_set_cdata_x(cc_xml_elt * elt, const char * data) {
  (void)elt; (void)data; // Suppress unused parameter warnings
}

const char * cc_xml_elt_get_cdata(const cc_xml_elt * elt) {
  (void)elt; // Suppress unused parameter warning
  return nullptr;
}

void cc_xml_elt_add_child_x(cc_xml_elt * elt, cc_xml_elt * child) {
  (void)elt; (void)child; // Suppress unused parameter warnings
}

cc_xml_elt * cc_xml_elt_get_child_of_type(const cc_xml_elt * elt, const char * type, int idx) {
  (void)elt; (void)type; (void)idx; // Suppress unused parameter warnings
  return nullptr;
}

int cc_xml_elt_get_num_children_of_type(const cc_xml_elt * elt, const char * type) {
  (void)elt; (void)type; // Suppress unused parameter warnings
  return 0;
}

// Debug function used only in SoGLDriverDatabase.cpp
void cc_xml_doc_write_to_file(cc_xml_doc * doc, const char * filename) {
  (void)doc; (void)filename; // Suppress unused parameter warnings
  // Stub implementation - do nothing
}