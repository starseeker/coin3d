#ifndef COIN_SOPROFILINGREPORTGENERATOR_H
#define COIN_SOPROFILINGREPORTGENERATOR_H

/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 * 
 * SoProfilingReportGenerator - profiling report generation utilities
 **************************************************************************/

#include <Inventor/C/basic.h>
#include <Inventor/SbString.h>
#include <Inventor/lists/SbList.h>

class SbProfilingData;
class SbProfilingReportSortCriteria;
class SbProfilingReportPrintCriteria;

// Forward declarations for callback types
typedef int ReportCB(void * userdata, int entryindex, const char * text);

class COIN_DLL_API SoProfilingReportGenerator {
public:
  enum DataCategorization {
    NODES,
    TYPES,
    NAMES
  };

  enum SortOrder {
    TIME_ASC,
    TIME_DES,
    TIME_MAX_ASC,
    TIME_MAX_DES,
    TIME_AVG_ASC,
    TIME_AVG_DES,
    COUNT_ASC,
    COUNT_DES,
    ALPHANUMERIC_ASC,
    ALPHANUMERIC_DES,
    MEM_ASC,
    MEM_DES,
    GFXMEM_ASC,
    GFXMEM_DES,
    GFX_MEM_ASC,
    GFX_MEM_DES
  };

  enum Column {
    NAME,
    TYPE,
    COUNT,
    TIME_SECS,
    TIME_SECS_MAX,
    TIME_SECS_AVG,
    TIME_MSECS,
    TIME_MSECS_MAX,
    TIME_MSECS_AVG,
    TIME_PERCENT,
    TIME_PERCENT_MAX,
    TIME_PERCENT_AVG,
    MEM,
    MEM_PERCENT,
    GFXMEM,
    GFXMEM_PERCENT,
    MEM_BYTES,
    MEM_KILOBYTES,
    GFX_MEM_BYTES,
    GFX_MEM_KILOBYTES
  };

  enum CallbackResponse {
    CONTINUE,
    STOP
  };

  static void init(void);
  static void cleanup(void);

  static void generate(const SbProfilingData & data,
                      DataCategorization categorization,
                      SbString & output);

  // Extended API for custom reports
  static SbProfilingReportSortCriteria * getReportSortCriteria(const SbList<SortOrder> & order);
  static SbProfilingReportSortCriteria * getDefaultReportSortCriteria(DataCategorization category);
  static void freeCriteria(SbProfilingReportSortCriteria * criteria);

  static SbProfilingReportPrintCriteria * getReportPrintCriteria(const SbList<Column> & order);
  static SbProfilingReportPrintCriteria * getDefaultReportPrintCriteria(DataCategorization category);
  static void freeCriteria(SbProfilingReportPrintCriteria * criteria);

  static void generate(const SbProfilingData & data,
                      DataCategorization categorization,
                      SbProfilingReportSortCriteria * sortcriteria,
                      SbProfilingReportPrintCriteria * printcriteria,
                      int numlines,
                      SbBool toplines,
                      ReportCB * reportcallback,
                      void * userdata);

  static CallbackResponse consoleReport(void * userdata, int entryindex);
  static CallbackResponse stringReport(void * userdata, int entryindex);
  static CallbackResponse stdoutCB(void * userdata, int entryidx, const char * text);
  static CallbackResponse stderrCB(void * userdata, int entryidx, const char * text);

protected:
  SoProfilingReportGenerator(void);
  ~SoProfilingReportGenerator();
};

#endif // !COIN_SOPROFILINGREPORTGENERATOR_H