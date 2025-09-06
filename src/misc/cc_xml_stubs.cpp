/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 * 
 * Minimal stub implementations for cc_xml functions to resolve linking errors
 **************************************************************************/

#include <Inventor/C/XML/document.h>
#include <Inventor/C/XML/element.h>
#include <cstdlib>

// Stub implementations for cc_xml_* functions
// These provide minimal functionality to resolve linking errors

cc_xml_doc * cc_xml_doc_new(void) {
  return nullptr;
}

void cc_xml_doc_delete_x(cc_xml_doc * doc) {
  // Stub implementation - do nothing
}

SbBool cc_xml_doc_read_file_x(cc_xml_doc * doc, const char * path) {
  return FALSE;
}

SbBool cc_xml_doc_read_buffer_x(cc_xml_doc * doc, const char * buffer, size_t buflen) {
  return FALSE;
}

cc_xml_elt * cc_xml_doc_get_root(const cc_xml_doc * doc) {
  return nullptr;
}

void cc_xml_doc_set_root_x(cc_xml_doc * doc, cc_xml_elt * root) {
  // Stub implementation - do nothing
}

cc_xml_elt * cc_xml_elt_new(void) {
  return nullptr;
}

cc_xml_elt * cc_xml_elt_new_from_data(const char * type, cc_xml_attr ** attrs) {
  return nullptr;
}

cc_xml_elt * cc_xml_elt_clone(const cc_xml_elt * elt) {
  return nullptr;
}

void cc_xml_elt_set_type_x(cc_xml_elt * elt, const char * type) {
  // Stub implementation - do nothing
}

const char * cc_xml_elt_get_type(const cc_xml_elt * elt) {
  return nullptr;
}

void cc_xml_elt_set_cdata_x(cc_xml_elt * elt, const char * data) {
  // Stub implementation - do nothing  
}

const char * cc_xml_elt_get_cdata(const cc_xml_elt * elt) {
  return nullptr;
}

void cc_xml_elt_add_child_x(cc_xml_elt * elt, cc_xml_elt * child) {
  // Stub implementation - do nothing
}

cc_xml_elt * cc_xml_elt_get_child_of_type(const cc_xml_elt * elt, const char * type, int idx) {
  return nullptr;
}

int cc_xml_elt_get_num_children_of_type(const cc_xml_elt * elt, const char * type) {
  return 0;
}