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

/*!
  \typedef bool SbBool
  \brief SbBool is a boolean type alias used throughout Obol.

  \ingroup coin_base

  SbBool is a \c typedef for the C++ \c bool type, maintained for
  API compatibility with Open Inventor 2.1 and the upstream Coin library.
  All values are standard C++ \c true / \c false.
*/


/* *********************************************************************** */

/*!
  \class SoDB SoDB.h Inventor/SoDB.h
  \brief The SoDB class keeps track of internal global data.

  \ingroup coin_general

  This class collects various methods for initializing, setting and
  accessing common global data from the Coin library.

  All methods on SoDB are static.

  Make sure you call SoDB::init() (either directly or indirectly
  through the init() method of the GUI glue library) before you use
  any of the other Coin classes.
*/

#include <Inventor/SoDB.h>

#include "config.h"

#include <cstdlib>
#include <cassert>
#include <cstring>
#include <cstdarg>

#ifdef HAVE_UNISTD_H
#include <unistd.h> // fd_set (?)
#endif // HAVE_UNISTD_H

#include "CoinTidbits.h"
#include "misc/SoEnvironment.h"
#include <Inventor/SbVec3f.h>
#include <Inventor/SoInput.h>
#include <Inventor/actions/SoAction.h>
#include <Inventor/details/SoDetail.h>
#include <Inventor/elements/SoElement.h>
#include <Inventor/engines/SoEngine.h>
#include <Inventor/engines/SoNodeEngine.h>
#include <Inventor/engines/SoEngineOutput.h>
#include <Inventor/errors/SoReadError.h>
#include <Inventor/events/SoEvent.h>
#include <Inventor/fields/SoSFTime.h>
#include <Inventor/misc/SoGLBigImage.h>
#include <Inventor/misc/SoGLImage.h>
#include <Inventor/misc/SoProto.h>
#include <Inventor/misc/SoProtoInstance.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/sensors/SoTimerSensor.h>
#include <Inventor/annex/HardCopy/SoHardCopy.h>
#include <Inventor/errors/SoDebugError.h>
#include <Inventor/annex/FXViz/nodes/SoShadowGroup.h>

#include <Inventor/misc/SoGLDriverDatabase.h>
#include <Inventor/misc/CoinResources.h>

#include "shaders/SoShader.h"
/* glue/glp.h include removed - no longer needed for old callback system */

#include "fields/SoGlobalField.h"
#include "misc/systemsanity.icc"
#include "misc/SoDBP.h"
#include "misc/SbHash.h"
#include "misc/SoConfigSettings.h"
#include "rendering/SoVBO.h"

// Threading support
#include "threads/threadp.h"

#ifdef OBOL_THREADSAFE
#include <Inventor/threads/SbRWMutex.h>
#include "threads/recmutexp.h"
#endif // OBOL_THREADSAFE

#ifdef HAVE_3DS_IMPORT_CAPABILITIES
#include "3ds/3dsLoader.h"
#endif // HAVE_3DS_IMPORT_CAPABILITIES

#include <Inventor/annex/Profiler/SoProfiler.h>
#include <Inventor/annex/Profiler/elements/SoProfilerElement.h>
#include "profiler/SoProfilerP.h"
#include <Inventor/annex/HUD/SoHUD.h>

/* Old OSMesa initialization code removed - now uses public SoDB::ContextManager API */

// Static storage for the global context manager
namespace {
  SoDB::ContextManager * global_context_manager = nullptr;
}

// *************************************************************************

// Coin-global envvars:

#ifndef DOXYGEN_SKIP_THIS
const char * SoDBP::EnvVars::OBOL_PROFILER = "OBOL_PROFILER";
const char * SoDBP::EnvVars::OBOL_PROFILER_OVERLAY = "OBOL_PROFILER_OVERLAY";
#endif // DOXYGEN_SKIP_THIS

// *************************************************************************

/*!
  \typedef void SoDBHeaderCB(void * data, SoInput * input)

  The type definition for the pre and post callback functions that may be
  specified for user defined headers. Note that for all internally defined headers
  callback functions are not used.
*/

// *************************************************************************

static SbString * coin_versionstring = NULL;

// atexit callback
static void cleanup_func(void)
{
  delete coin_versionstring;
  coin_versionstring = NULL;
}


// *************************************************************************

// For sanity checking that our static variables in Coin have had a
// chance to init themselves before the invocation of SoDB::init().
//
// At least under Windows, it is possible to force the compiler /
// linker / system loader to init static application objects /
// variables before the ones in DLLs. If this is done (as we have seen
// with at least one external user), various hard-to-debug problems
// may crop up.

// Canary value: used to detect DLL init order problems.  On platforms where
// DLL constructors may run in an unexpected order, global statics in Obol
// might not be initialized before SoDB::init() is called.  If this value is
// not 0xdeadbeef when init() runs, it means the DLL was not initialized first.
static uint32_t a_static_variable = 0xdeadbeef;

// *************************************************************************

/*!
  Initialize the Coin library with the provided OpenGL context manager.

  This function sets up the Coin library with the specified context manager
  for OpenGL operations. The context manager must be provided and will be
  used for all offscreen rendering operations throughout the library's lifetime.

  Note that this function should be called before using any other Coin
  class or function. If the Coin library is built as a DLL under Microsoft
  Windows and you are experiencing problems with initializing the library,
  please see the class documentation of SoDB for a description of how to
  work around this problem.

  It's safe to call this function multiple times with the same context manager.

  Make sure you call SoDB::cleanup() before application termination, for
  the Coin library to be able to clean up internal static data structures.

  \param context_manager The OpenGL context manager to use for rendering operations.
                        Must not be NULL.

  \sa cleanup(), finish()

  \since Coin 4.0 (breaking change - context manager is now required)
 */
void
SoDB::init(ContextManager * context_manager)
{
  OBOL_INIT_CHECK_THREAD();

  // Require a valid context manager
  if (!context_manager) {
    // Use fprintf here because SoError::initClasses() has not been called yet;
    // calling SoDebugError::post() before that would trigger an assert.
    fprintf(stderr, "SoDB::init: Context manager is NULL. "
                    "Applications must provide a valid ContextManager implementation. "
                    "Proceeding with limited functionality.\n");
    // For internal library calls, we proceed but with limited context support
    // Applications should always provide a proper context manager
  } else {
    // Set the global context manager
    global_context_manager = context_manager;
  }

  // Allow re-initialization if a context manager is provided, even if already initialized
  // This enables tests to be run in isolation with proper context setup
  if (SoDB::isInitialized() && !context_manager) {
    return; // No re-initialization without context manager
  }

  // If already initialized and context manager is provided, just update the context manager
  if (SoDB::isInitialized() && context_manager) {
    global_context_manager = context_manager;
    return; // Context manager updated, no need to re-initialize everything else
  }

  // This is to catch the (unlikely) event that the C++ compiler adds
  // padding or rtti information to the SbVec3f (or similar) base classes.
  // We assume this isn't done several places in Coin, so the best thing to
  // do is just to assert here.
  assert(sizeof(SbVec3f) == 3*sizeof(float));

  // Sanity check that our static variables in Coin have had a chance
  // to init themselves before the first invocation of this function
  // happens. See above documentation on the variable for more
  // information.
  assert((a_static_variable == 0xdeadbeef) &&
         "SoDB::init() called before Coin DLL initialization!");
  (void)a_static_variable; // suppress unused variable warning in release builds

  // Initialize threading subsystem first as it's needed for other components
  cc_thread_init();

  // See systemsanity.icc
  SoDB_checkGCCBuiltinExpectSanity();

  // See SbBasic.h, notes about bug in the Sun CC 4.0 compiler vs
  // probable bug in the Intel compiler.
  assert(SoNode::getClassTypeId() == SoType::badType() && "Data init failed! Get in touch with maintainers at <coin-support@coin3d.org>");

  // Note: variableArgsSanityCheck() was removed as it's no longer needed.
  // The underlying cc_string_vsprintf() function now uses modern C++17 
  // std::vsnprintf with proper va_copy handling, eliminating the variable
  // argument reuse issues that the sanity check was designed to detect.


#ifdef HAVE_THREADS
  // Modern C++17 threading doesn't require explicit initialization
#ifdef OBOL_THREADSAFE
  SoDBP::globalmutex = new SbRWMutex(SbRWMutex::READ_PRECEDENCE);
#endif // OBOL_THREADSAFE
#endif // HAVE_THREADS

  coin_init_tidbits();

  // Allocate our static members.  These are freed in SoDBP::clean(),
  // which is registered with coin_atexit() below.
  SoDBP::headerlist = new SbList<SoDB_HeaderInfo *>;
  SoDBP::sensormanager = new SoSensorManager;
  SoDBP::converters = new UInt32ToInt16Map;

  // NB! There are dependencies in the order of initialization of
  // components below.

  // This obviously needs to be done first.
  SoType::init();

  // Error classes must be initialized before we do the type size
  // checking below, as we spit out warning messages if
  // inconsistencies are found.
  SoError::initClasses();


  SoConfigSettings::getInstance();

  CoinResources::init();
  SoInput::init();
  SoBase::initClass();
  SoDetail::initClass();
  // SoPath inherits SoBase, so initialize it after SoBase.
  SoPath::initClass();
  SoFieldContainer::initClass();
  SoGlobalField::initClass(); // depends on SoFieldContainer init
  SoField::initClass();

  // Elements must be initialized before actions.
  SoElement::initClass();

  // The profiler-elements must also be initialized before actions, of
  // course. (Note that at least the first one *must* be initialized
  // even if OBOL_PROFILER is not set, as we use its classStackIndex
  // when checking if its present on the state stack.)
  SoProfilerElement::initClass();

  // ScXML classes removed - navigation now uses direct C++ APIs

  // Actions must be initialized before nodes (because of SO_ENABLE)
  SoAction::initClass();
  SoNode::initClass();
  SoEngine::initClass();
  SoEvent::initClass();
  SoSensor::initClass();

  SoProto::initClass();
  SoProtoInstance::initClass();

  SoGLDriverDatabase::init();
  SoGLImage::initClass();
  SoGLBigImage::initClass();

  SoHardCopy::init();

  SoShader::init();
  SoVBO::init();
  SoShadowGroup::init();



  // Register all valid file format headers.
  SoDB::registerHeader(SbString("#Inventor V2.1 ascii   "), FALSE, 2.1f,
                       NULL, NULL, NULL);
  SoDB::registerHeader(SbString("#Inventor V2.1 binary  "), TRUE, 2.1f,
                       NULL, NULL, NULL);

  // Register v2.4/2.5 Inventor file headers.  Obol handles these by reading
  // the compatible subset; TGS-specific nodes may not load correctly.
  SoDB::registerHeader(SbString("#Inventor V2.4 ascii   "), FALSE, 2.4f,
                       NULL, NULL, NULL);
  SoDB::registerHeader(SbString("#Inventor V2.4 binary  "), TRUE, 2.4f,
                       NULL, NULL, NULL);
  SoDB::registerHeader(SbString("#Inventor V2.5 ascii   "), FALSE, 2.5f,
                       NULL, NULL, NULL);
  SoDB::registerHeader(SbString("#Inventor V2.5 binary  "), TRUE, 2.5f,
                       NULL, NULL, NULL);

  SoDB::registerHeader(SbString("#Inventor V2.0 ascii   "), FALSE, 2.0f,
                       NULL, NULL, NULL);
  SoDB::registerHeader(SbString("#Inventor V2.0 binary  "), TRUE, 2.0f,
                       NULL, NULL, NULL);

  // Register v1.0 Inventor file headers for basic compatibility.
  // Not all v1.x constructs are supported; files may need conversion
  // to v2.1 for full fidelity.
  SoDB::registerHeader(SbString("#Inventor V1.0 ascii   "), FALSE, 1.0f,
                       NULL, NULL, NULL);
  SoDB::registerHeader(SbString("#Inventor V1.0 binary  "), TRUE, 1.0f,
                       NULL, NULL, NULL);

  SoDB::createGlobalField("realTime", SoSFTime::getClassTypeId());
  SoGlobalField::getGlobalFieldContainer("realTime")->ref();

  // use a callback to remove the realTime field
  coin_atexit((coin_atexit_f *)SoDBP::removeRealTimeFieldCB, CC_ATEXIT_REALTIME_FIELD);

  SoDBP::globaltimersensor = new SoTimerSensor;
  SoDBP::globaltimersensor->setFunction(SoDBP::updateRealTimeFieldCB);
  // An interval of 1/12 is pretty low for today's standards, but this
  // won't result in e.g. jerky animations of SoRotor & Co, since the
  // SoSensorManager automatically and continually re-triggers scene
  // traversals when anything has been connected to the realTime
  // field.
  SoDBP::globaltimersensor->setInterval(SbTime(1.0/12.0));
  // The sensor is scheduled unconditionally; it will only trigger
  // actual traversals when the realTime field has dependents.
  // Drift in the timer is acceptable since SoSensorManager re-triggers
  // traversals continuously when the scene is animated.
  SoDBP::globaltimersensor->schedule();

  // Force correct time on first getValue() from "realTime" field.
  SoDBP::updateRealTimeFieldCB(NULL, NULL);

  // This should prove helpful for debugging the pervasive problem
  // under Win32 with loading multiple instances of the same library.
  //
  // Note that this is done late in the init()-process, to make sure
  // all Coin-features used in SoDB::listWin32ProcessModules() have
  // been initialized.
  auto env = CoinInternal::getEnvironmentVariable("OBOL_DEBUG_LISTMODULES");
  if (env.has_value() && (std::atoi(env->c_str()) > 0)) { SoDBP::listWin32ProcessModules(); }

  SoDBP::isinitialized = TRUE;

  // NOTE: SoDBP::isinitialized must be set to TRUE before this block,
  // or you will get a "mysterious" crash on a mutex. Logically, it should
  // not be flagged before initialization is done, but subsystems invoked
  // from these methods need to know that Coin is already initialized.
  SoProfilerP::parseCoinProfilerVariable();
  if (SoProfiler::isEnabled()) {
    SoProfiler::init();
  }

  SoHUD::init();

  // Note: OSMesa context initialization has been moved to test applications
  // Applications must provide context creation callbacks via SoGLContext_context_set_offscreen_cb_functions()
  // See examples/osmesa_example.h for reference implementation

  // Debugging for memory leaks will be easier if we can clean up the
  // resource usage. This needs to be done last in init(), so we get
  // called _before_ clean() methods in other classes with the same
  // priority.
  coin_atexit((coin_atexit_f *)SoDBP::clean, CC_ATEXIT_SODB);
}

/*!
  Invoke this method as the last call of the application code, to
  trigger a cleanup of all static resources used by the Coin library.

  This is usually not necessary for standalone executable
  applications, as the operating system will take care of cleaning up
  after the process as it exits.

  It may be necessary to invoke this method to avoid leaks for
  "special" execution environments, though, like if the Coin library
  is used as e.g. a browser plug-in, or some other type of component
  which can be started, shut down and restarted multiple times.

  \since Coin 2.4
  \since TGS Inventor 5.0
*/
void
SoDB::finish(void)
{
  coin_atexit_cleanup();
  SoDBP::isinitialized = FALSE;
}

/*!
  This method was renamed from Coin version 2.4 onwards, to
  SoDB::finish(). Consider this name for the method obsolete.

  \since Coin 2.0
*/
void
SoDB::cleanup(void)
{
  SoDB::finish();
}

/*!
  Returns a text string containing the name of the library and version
  information.
*/
const char *
SoDB::getVersion(void)
{
  if (coin_versionstring == NULL) {
    coin_versionstring = new SbString("SIM Coin " OBOL_VERSION);
    coin_atexit((coin_atexit_f *)cleanup_func, CC_ATEXIT_NORMAL);
  }
  return coin_versionstring->getString();
}

/*!
  Instantiates and reads an SoPath object from \a in and returns a
  pointer to it in \a path.

  The reference count of the SoPath object will initially be zero.

  Returns \c FALSE on error. Returns \c TRUE with \a path equal to \a
  NULL if we hit end of file instead of a new path specification in
  the file.
*/
SbBool
SoDB::read(SoInput * in, SoPath *& path)
{
  path = NULL;
  SoBase * baseptr;
  if (!SoDB::read(in, baseptr))  return FALSE;
  if (!baseptr) return TRUE; // eof

  if (!baseptr->isOfType(SoPath::getClassTypeId())) {
    SoReadError::post(in, "'%s' not derived from SoPath",
                      baseptr->getTypeId().getName().getString());
    baseptr->ref();
    baseptr->unref();
    return FALSE;
  }

  path = (SoPath *)baseptr;
  return TRUE;
}

/*!
  Instantiates and reads an object of type SoBase from \a in and
  returns a pointer to it in \a base. \a base will be \c NULL on
  return if we hit end of file.

  The reference count of the base object will initially be zero.

  Returns \c FALSE on error.
*/
SbBool
SoDB::read(SoInput * in, SoBase *& base)
{
  SbBool valid = in->isValidFile();

  if (!valid && SoDBP::is3dsFile(in)) {
    base = SoDBP::read3DSFile(in);
    return (base != NULL);
  }

  // Header is only required when reading from a stream, if reading from
  // memory no header is required.
  if (!valid) {
    return FALSE;
  }
  return SoBase::read(in, base, SoBase::getClassTypeId());
}

/*!
  Instantiates and reads an object of type SoNode from \a in and returns
  a pointer to it in \a rootnode.

  The reference count of the node will initially be zero.

  Returns \c FALSE on error. Returns \c TRUE with \a rootnode equal to
  \c NULL if we hit end of file instead of a new node specification in
  the file.
 */
SbBool
SoDB::read(SoInput * in, SoNode *& rootnode)
{
  rootnode = NULL;
  SoBase * baseptr;

  if (SoDBP::is3dsFile(in)) {
    rootnode = SoDBP::read3DSFile(in);
    return (rootnode != NULL);
  }

  // allow engines at the top level of a file
  do {
    if (!SoDB::read(in, baseptr)) return FALSE;
    if (!baseptr) return TRUE; // eof
  } while (baseptr->isOfType(SoEngine::getClassTypeId()));

  if (!baseptr->isOfType(SoNode::getClassTypeId())) {
    SoReadError::post(in, "'%s' not derived from SoNode",
                      baseptr->getTypeId().getName().getString());
    baseptr->ref();
    baseptr->unref();
    return FALSE;
  }

  rootnode = (SoNode *)baseptr;
  return TRUE;
}

/*!
  Reads all graphs from \a in and returns them under an SoSeparator
  node. If the file contains only a single graph under an SoSeparator
  node (which is the most common way of constructing and exporting
  scene graphs), no \e extra SoSeparator root node will be made, but
  the returned root node will be the top-most node from the file.

  The reference count of the root separator returned from this method
  will be zero. Other nodes in the returned scene graph will have
  reference counts according to the number of parent-child
  relationships, as usual.

  The common layout for how to load, work with and then finally
  destruct and return memory resources of scene graphs usually goes
  like this:

  \code
  // [snip]
  SoInput in;
  if (!in.openFile(filename)) { exit(1); }

  SoSeparator * root = SoDB::readAll(&in);
  if (!root) { exit(1); }

  // root-node returned from SoDB::readAll() has initial zero
  // ref-count, so reference it before we start using it to
  // avoid premature destruction.
  root->ref();

  // [do your thing here -- attach the scene to a viewer or whatever]

  // Bring ref-count of root-node back to zero to cause the
  // destruction of the scene.
  root->unref();
  // (Upon self-destructing, the root-node will also dereference
  // its children nodes, so they also self-destruct, and so on
  // recursively down the scene graph hierarchy until the complete
  // scene graph has self-destructed and thereby returned all
  // memory resources it was using.)
  \endcode

  Returns \c NULL on any error.

  Tip: a common operation to do after importing a scene graph is to
  pick out the memory pointers to one or more of the imported nodes
  for further handling. This can be accomplished by using either the
  SoNode::getByName() function (which is the easier approach) or by
  using an instance of the SoSearchAction class (which is the more
  complex but also more flexible approach).
*/
SoSeparator *
SoDB::readAll(SoInput * in)
{
  return (SoSeparator*)
    SoDB::readAllWrapper(in, SoSeparator::getClassTypeId());
}

/*!
  Check if \a testString is a valid file format header identifier string.

  \sa getHeaderData(), registerHeader()
 */
SbBool
SoDB::isValidHeader(const char * teststring)
{
  SbBool isbinary;
  float ivversion;
  SoDBHeaderCB * preload_cb, * postload_cb;
  void * userdata;

#if OBOL_DEBUG
  if (!teststring) {
    SoDebugError::postWarning("SoDB::isValidHeader",
                              "Passed a NULL string pointer.");
    return FALSE;
  }
#endif // OBOL_DEBUG

  return SoDB::getHeaderData(SbString(teststring), isbinary, ivversion,
                             preload_cb, postload_cb, userdata, TRUE);
}

/*!
  Register a header string which should be recognized by SoInput upon
  file import. This is a convenient way for library users to register
  their own Coin derived file formats.

  Set \a isbinary to \c TRUE if the file should be read as binary
  data, and set \a ivversion to indicate which Coin library version is
  needed to read the file.

  Callbacks \a precallback and \a postcallback will be called before
  and after importing the custom format.

  If \a headerstring cannot be accepted as a valid file format header
  for Coin files, \c FALSE will be returned. A valid header \e must
  start with a '#' character, and not be more than 80 characters long.

  \sa getHeaderData()
*/
SbBool
SoDB::registerHeader(const SbString & headerstring,
                     SbBool isbinary, float ivversion,
                     SoDBHeaderCB * precallback, SoDBHeaderCB * postcallback,
                     void * userdata)
{
  // Must start with '#'.
  if ((headerstring.getLength() == 0) ||
      (headerstring.getString()[0] != '#')) {
#if OBOL_DEBUG
    SoDebugError::postWarning("SoDB::registerHeader",
                              "File header string must begin with '#', '%s'"
                              " invalid.", headerstring.getString());
#endif // OBOL_DEBUG
    return FALSE;
  }

  // No more than 80 characters.
  if (headerstring.getLength() > 80) {
#if OBOL_DEBUG
    SoDebugError::postWarning("SoDB::registerHeader",
                              "File header string must be 80 characters "
                              "or less, so '%s' is invalid.",
                              headerstring.getString());
#endif // OBOL_DEBUG
    return FALSE;
  }

  SoDB_HeaderInfo * newheader =
    new SoDB_HeaderInfo(headerstring, isbinary, ivversion,
                        precallback, postcallback, userdata);
  SoDBP::headerlist->append(newheader);
  return TRUE;
}

/*!
  Returns the settings for the given \a headerstring, if \a headerstring
  is a valid header.

  If \a substringok is \c TRUE, ignore trailing characters in \a headerstring
  when checking for validity.

  If no valid header string by this name is found, \c FALSE is returned,
  otherwise \c TRUE will be returned, and the other input arguments will
  be set to their respective values.

  \sa isValidHeader(), registerHeader()
 */
SbBool
SoDB::getHeaderData(const SbString & headerstring, SbBool & isbinary,
                    float & ivversion, SoDBHeaderCB *& precallback,
                    SoDBHeaderCB *& postcallback, void *& userdata,
                    SbBool substringok)
{
  unsigned int hslen = headerstring.getLength();
  if (hslen == 0) return FALSE;

  // Disregard whitespace.
  while ((headerstring[hslen-1] == ' ') || (headerstring[hslen-1] == '\t')) {
    hslen--;
    if (hslen == 0) return FALSE;
  }

  SbString tryheader = headerstring.getSubString(0, hslen-1);

  SbBool hit = FALSE;
  for (int i=0; (i < SoDB::getNumHeaders()) && !hit; i++) {
    SoDB_HeaderInfo * hi = (*SoDBP::headerlist)[i];
    SbString & s = hi->headerstring;
    unsigned int reglen = s.getLength();
    assert(reglen > 0);
    // Disregard whitespace.
    while ((s[reglen-1] == ' ') || (s[reglen-1] == '\t')) reglen--;
    assert(reglen > 0);

    SbString regheader = s.getSubString(0, reglen-1);

    if (regheader == tryheader)
      hit = TRUE;
    else if (substringok && (hslen > reglen) &&
             (regheader == tryheader.getSubString(0, reglen - 1)))
      hit = TRUE;

    if (hit) {
      isbinary = hi->isbinary;
      ivversion = hi->ivversion;
      precallback = hi->preload_cb;
      postcallback = hi->postload_cb;
      userdata = hi->userdata;
    }
  }

  return hit;
}

/*!
  Returns number of registered file headers.

  \sa getHeaderString()
 */
int
SoDB::getNumHeaders(void)
{
  return SoDBP::headerlist->getLength();
}

/*!
  Returns the identifier header string of index \a i.

  \sa getNumHeaders(), getHeaderData()
 */
SbString
SoDB::getHeaderString(const int i)
{
#if OBOL_DEBUG
  if ((i < 0) || (i >= SoDBP::headerlist->getLength())) {
    SoDebugError::post("SoDB::getHeaderString", "Index %d out of range.", i);
    return SbString("");
  }
#endif // OBOL_DEBUG

  return (*SoDBP::headerlist)[i]->headerstring;
}

/*!
  Create a new global field by the given \a type, and identified in
  subsequent accesses to getGlobalField() by \a name. If a global
  field by the name and type already exists, returns a pointer to it.
  If a global field with the same name but a different type exists,
  returns \c NULL.

  A global field can be deallocated by calling
  SoDB::renameGlobalField(), with the second argument set to an empty
  string.

  \sa getGlobalField(), renameGlobalField()
 */
SoField *
SoDB::createGlobalField(const SbName & name, SoType type)
{
  assert(name != "" && "invalid name for a global field");

  SoField * f = SoDB::getGlobalField(name);
  if (f) {
    if (f->getTypeId() == type) return f;
    else return NULL;
  }

#if OBOL_DEBUG
  if (!type.canCreateInstance()) {
    SoDebugError::postWarning("SoDB::createGlobalField",
                              "Can't create instance of field type \"%s\".",
                              type.getName().getString());
    return NULL;
  }
#endif // OBOL_DEBUG

  // (Deallocation of the field happens in the SoGlobalField
  // destructor.)
  SoField * newfield = (SoField *)type.createInstance();
  (void) new SoGlobalField(name, newfield);
  return newfield;
}

/*!
  If there exist a global field with the given \a name, return a
  pointer to it. If there is no field with this name, return \c NULL.

  Of particular interest is the \c realTime global field set up by the
  library on initialization. This field is used as a source field to
  all the autonomous animation objects within the library, like for
  instance the SoTimeCounter engine or the SoRotor node.

  If you want to control the speed of "action" of a scene with
  animating / moving components (for instance for doing fixed
  frame-time snapshots for generating a movie), grab the global field
  named "realTime" and use it in the manner depicted in the class
  documentation of the SoOffscreenRenderer class.

  \sa createGlobalField(), renameGlobalField()
*/
SoField *
SoDB::getGlobalField(const SbName & name)
{
  SoGlobalField * gf = SoGlobalField::getGlobalFieldContainer(name);
  return gf ? gf->getGlobalField() : NULL;
}

/*!
  Rename a global field. If \a to is an empty name, the \a from field
  gets deleted. If another global field already goes by the name \a
  to, that field will get deleted before the rename operation.

  \sa getGlobalField(), createGlobalField()
*/
void
SoDB::renameGlobalField(const SbName & from, const SbName & to)
{
  SoGlobalField * gf = SoGlobalField::getGlobalFieldContainer(from);

#if OBOL_DEBUG
  if (gf == NULL) {
    SoDebugError::postWarning("SoDB::renameGlobalField",
                              "Couldn't find global field '%s' to rename.",
                              from.getString());
    return;
  }
#endif // OBOL_DEBUG

  if (to == "") { // Empty string is a special case, remove field.
    assert(gf->getRefCount() == 1);
    SoGlobalField::removeGlobalFieldContainer(gf);
    return;
  }

  // Existing entry by the same "to"-name? If so, remove it.
  SoGlobalField * old = SoGlobalField::getGlobalFieldContainer(to);
  if (old) {
    assert(old->getRefCount() == 1);
    SoGlobalField::removeGlobalFieldContainer(old);
  }

  gf->setName(to);
}

/*!
  Set the time interval between updates for the \c realTime global field
  to \a interval. Default value is 1/12 s.

  The low update rate is due to historical reasons, to be compatible
  with application code written for SGI Inventor.

  Setting the interval to a zero time will disable automatic updates
  of the realTime field.

  \sa getRealTimeInterval(), getGlobalField()
 */
void
SoDB::setRealTimeInterval(const SbTime & interval)
{
#if OBOL_DEBUG
  if (interval < SbTime::zero()) {
    SoDebugError::postWarning("SoDB::setRealTimeInterval",
                              "Tried to set negative interval.");
    return;
  }
#endif // OBOL_DEBUG

  SbBool isscheduled = SoDBP::globaltimersensor->isScheduled();
  if (isscheduled) SoDBP::globaltimersensor->unschedule();
  SoDBP::globaltimersensor->setInterval(interval);
  if (isscheduled && interval != SbTime::zero())
    SoDBP::globaltimersensor->schedule();
}

/*!
  Returns the current trigger interval for the global \a realTime SbTime
  field.

  \sa setRealTimeInterval(), getGlobalField()
 */
const SbTime &
SoDB::getRealTimeInterval(void)
{
  return SoDBP::globaltimersensor->getInterval();
}

/*!
  This is just a wrapper for the method in SoSensorManager by the
  same name.

  \sa getDelaySensorTimeout(), SoSensorManager
 */
void
SoDB::setDelaySensorTimeout(const SbTime & t)
{
  SoDB::getSensorManager()->setDelaySensorTimeout(t);
}

/*!
  This is just a wrapper for the method in SoSensorManager by the
  same name.

  \sa setDelaySensorTimeout(), SoSensorManager
 */
const SbTime &
SoDB::getDelaySensorTimeout(void)
{
  return SoDB::getSensorManager()->getDelaySensorTimeout();
}

/*!
  Returns a pointer to the global sensor manager. The sensor manager keeps
  track of the sensor queues.
 */
SoSensorManager *
SoDB::getSensorManager(void)
{
  return SoDBP::sensormanager;
}

/*!
  Notify SoDB that there exists a way to convert data from the \a from
  SoField type to the \a to SoField type, by connecting them with an
  instance of the \a converter SoFieldConverter type.

  By doing this, SoDB::getConverter() will later be able to
  automatically return the type of the correct conversion class when
  requested.

  Coin internally provides conversion between most field types, so
  application programmers should usually not need to use this
  function. The exception is if you are writing your own field type
  classes, and want to be able to connect them to the internal field
  types (or other extensions field types).

  \sa createConverter(), SoFieldConverter
 */
void
SoDB::addConverter(SoType from, SoType to, SoType converter)
{
  const uint32_t linkid = (((uint32_t)from.getKey()) << 16) + to.getKey();
  SbBool nonexist = SoDBP::converters->put(linkid, converter.getKey());
  if (!nonexist) {
#if OBOL_DEBUG
    SoDebugError::postWarning("SoDB::addConverter",
                              "Conversion from \"%s\" to \"%s\" is already "
                              "handled by instances of \"%s\"",
                              from.getName().getString(),
                              to.getName().getString(),
                              converter.getName().getString());
#endif // OBOL_DEBUG
  }
}

/*!
  Return the type of an SoFieldConverter class which is able to
  convert data between fields of type \a from to the data field(s) of
  field type \a to.

  If no conversion between the given field types is possible, returns
  SoType::badType().

  \sa addConverter()
 */
SoType
SoDB::getConverter(SoType from, SoType to)
{
  uint32_t val = (((uint32_t)from.getKey()) << 16) + to.getKey();
  int16_t key;
  if (!SoDBP::converters->get(val, key)) { return SoType::badType(); }
  return SoType::fromKey(key);
}

/*!
  Returns \c TRUE if init() has been called.

  \sa init()
 */
SbBool
SoDB::isInitialized(void)
{
  return SoDBP::isinitialized;
}

/*!
  \COININTERNAL
 */
void
SoDB::startNotify(void)
{
#ifdef OBOL_THREADSAFE
  (void) cc_recmutex_internal_notify_lock();
#endif // OBOL_THREADSAFE
  SoDBP::notificationcounter++;
}

/*!
  \COININTERNAL
 */
SbBool
SoDB::isNotifying(void)
{
  return SoDBP::notificationcounter > 0;
}

/*!
  \COININTERNAL
 */
void
SoDB::endNotify(void)
{
  SoDBP::notificationcounter--;
  if (SoDBP::notificationcounter == 0) {
    // Process zero-priority sensors after notification has been done.
    SoSensorManager * sm = SoDB::getSensorManager();
    if (sm->isDelaySensorPending()) sm->processImmediateQueue();
  }
#ifdef OBOL_THREADSAFE
  (void) cc_recmutex_internal_notify_unlock();
#endif // OBOL_THREADSAFE

}

/*!
  Turn on or off the real time sensor.

  The most common use for turning the real time sensor off is to
  control the realTime global field from the user application. This is
  for instance handy when you want to take screen snapshots at fixed
  intervals. See the class documentation of SoOffscreenRenderer for
  further information.

  \sa setRealTimeInterval(), getGlobalField()
*/
void
SoDB::enableRealTimeSensor(SbBool on)
{
  assert(SoDB::isInitialized());

  SbBool isscheduled = SoDBP::globaltimersensor->isScheduled();
  if (isscheduled && !on) SoDBP::globaltimersensor->unschedule();
  else if (!isscheduled && on &&
           SoDBP::globaltimersensor->getInterval() != SbTime::zero())
    SoDBP::globaltimersensor->schedule();
#if OBOL_DEBUG
  else SoDebugError::postWarning("SoDB::enableRealTimeSensor",
                                 "realtime sensor already %s",
                                 on ? "on" : "off");
#endif // OBOL_DEBUG
}

// private wrapper for readAll() and readAllVRML()
SoGroup *
SoDB::readAllWrapper(SoInput * in, const SoType & grouptype)
{
  assert(SoDB::isInitialized() && "you forgot to initialize the Coin library");
  assert(grouptype.canCreateInstance());
  assert(grouptype.isDerivedFrom(SoGroup::getClassTypeId()));

  SbBool valid = in->isValidFile();

  if (!valid && SoDBP::is3dsFile(in)) {
    SoSeparator * root3ds = SoDBP::read3DSFile(in);
    if (root3ds == NULL) { return NULL; }

    if (!SoSeparator::getClassTypeId().isDerivedFrom(grouptype)) {
      SoGroup * root = (SoGroup *)grouptype.createInstance(in->getContextManager());
      root->addChild(root3ds);
      return root;
    }
    else {
      return root3ds;
    }
  }

  if (!valid) {
    SoReadError::post(in, "Not a valid Inventor file.");
    return NULL;
  }

#ifndef NDEBUG
  const int stackdepth = in->filestack.getLength();
#endif

  SoGroup * root = (SoGroup *)grouptype.createInstance(in->getContextManager());
  SoNode * topnode;
  do {
    if (!SoDB::read(in, topnode)) {
      root->ref();
      root->unref();
      return NULL;
    }
    if (topnode) { root->addChild(topnode); }
  } while (topnode && in->skipWhiteSpace());

  if (!in->eof()) {
    // All  characters  may not  have  been  read  from the  current
    // stream.  The reading stops when no more valid identifiers are
    // found, so we have to read until the current file on the stack
    // is at the end.  All non-whitespace characters from now on are
    // erroneous.
    static uint32_t readallerrors_termination = 0;
    char dummy = -1; // Set to -1 to make sure the variable has been
                     // read before an error is output
    char buf[2];
    buf[1] = '\0';
    while (!in->eof() && in->read(dummy)) {
      if (readallerrors_termination < 1) {
        buf[0] = dummy;
        SoReadError::post(in, "Erroneous character(s) after end of scene graph: \"%s\". "
                          "This message will only be shown once for this file, "
                          "but more errors might be present", dummy != '\0' ? buf : "\\0");
      }

      readallerrors_termination++;
    }
    assert(in->eof());

    if (dummy == '\0' && !in->isBinary()) {
      SoReadError::post(in, "It appears that your iv-file ends with a null-character ('\\0') "
                        "This could happen if you use the SoInput::setBuffer method "
                        "with a character-string argument and the size of the string "
                        "was one character too long.  A typical reason for the problem "
                        "is if you use sizeof to measure the string length; not taking into "
                        "account that strings end with '\\0', which should not be "
                        "input to the setBuffer-method. To correct this, use strlen "
                        "instead of sizeof.");
    }
  }

  // Make sure the current file (which is EOF) is popped off the stack.
  in->popFile(); // No popping happens if there is just one file on
                 // the stack

  // Detect problems with missing pops from the SoInput file stack.
#ifndef NDEBUG
  assert((stackdepth == 1 && in->filestack.getLength() == 1) ||
         (stackdepth - 1 == in->filestack.getLength()));
#endif

  // Strip off extra root group node if it was unnecessary (i.e. if
  // the file only had a single top-level root, and it was of the same
  // type as is requested returned).
  if ((root->getNumChildren() == 1) && (root->getChild(0)->isOfType(grouptype))) {
    SoNode * n = root->getChild(0);
    n->ref();
    root->ref();
    root->unref();
    n->unrefNoDelete();
    return (SoGroup *)n;
  }

  return root;
}

/* *********************************************************************** */

/*!
  \typedef SbBool SoDB::ProgressCallbackType(const SbName & itemid, float fraction, SbBool interruptible, void * userdata)

  Client code progress callback function must be static functions of
  this type.

  The \a itemid argument is a unique text identifier which says what
  is being processed (use this for any GUI progress bar informational
  text), and \a fraction is a value in the range [0, 1] which
  indicates how far the process has got. If the task is successfully
  aborted, the callback will be invoked a last time with \a fraction
  set to -1.0.

  The return value is an abort flag indication from the client
  code. Note that the process that is being run can only be aborted if
  the \a interruptible flag is set.

  See SoDB::addProgressCallback() for full documentation of how the
  progress notification mechanism works.
*/

/*!
  The concept behind progress information in Coin is that any internal
  process which may take a long time to complete (like e.g. file
  import for huge scenes) can pass on progress information by calling
  back to a progress callback set up by the code of the client
  application.

  The client's progress callback's function signature must match the
  SoDB::ProgressCallbackType.

  The mechanism works by enforcing that all progress notification from
  within Coin must

  <ol>

  <li> Use a unique text id to identify the "progress-informing"
       process (e.g. "File import" for SoDB::readAll() / SoInput file
       reading, "File export" for SoOutput / SoWriteAction, etc). This
       is the \a itemid name passed on to the progress callback.</li>

  <li>The first invocation of the user callback will be done with an
      exact 0.0 \a fraction value.</li>

  <li>The last invocation will be done with an exact 1.0 fraction
      value.</li>

  <li>An exception to the last point is that if the process is
      aborted, a final invocation with a -1.0 fraction value will be
      made.</li>

  </ol>

  One important thing to note about the mechanism is that processes
  with progress callbacks can be running within \e other processes
  using the progress callback functionality.  Progress information
  will then have to be considered to be "stacked", and client code
  must be aware of and treat this properly.

  \OBOL_FUNCTION_EXTENSION

  \since Coin 2.2
*/
void
SoDB::addProgressCallback(ProgressCallbackType * func, void * userdata)
{
  if (SoDBP::progresscblist == NULL) {
    SoDBP::progresscblist = new SbList<SoDBP::ProgressCallbackInfo>;
  }

  const SoDBP::ProgressCallbackInfo newitem = { func, userdata };
  SoDBP::progresscblist->append(newitem);
}

/*!
  Removes a progress callback function, which will no longer be
  invoked.
*/
void
SoDB::removeProgressCallback(ProgressCallbackType * func, void * userdata)
{
  assert(SoDBP::progresscblist);

  const SoDBP::ProgressCallbackInfo item = { func, userdata };
  const int idx = SoDBP::progresscblist->find(item);
  assert(idx != -1);
  SoDBP::progresscblist->remove(idx);
}

/*!
  Returns \c TRUE if this is a thread safe version of Coin
  (i.e. it was configured and built with --enable-threadsafe).
*/
SbBool
SoDB::isMultiThread(void)
{
#ifdef OBOL_THREADSAFE
  return TRUE;
#else // OBOL_THREADSAFE
  return FALSE;
#endif // !OBOL_THREADSAFE
}

// Note that the function names of the next four functions below are
// all lowercase to be compatible with client code written on TGS
// Inventor.

/*!

  Places a read lock on the global SoDB mutex. This can be used to
  synchronize between threads that are reading/writing Coin scene
  graphs.

  If you call this function, you must make sure that you also call
  SoDB::readunlock(). If you fail to do this, you might experience
  that your application locks up.

  All Coin actions have a read-lock on the global SoDB mutex while
  traversing the scene graph.

  \sa SoDB::readunlock(), SoDB::writelock()

  \since Coin 2.3
  \since TGS Inventor 3.0
*/
void
SoDB::readlock(void)
{
#ifdef OBOL_THREADSAFE
  SoDBP::globalmutex->readLock();
#endif // OBOL_THREADSAFE
}

/*!
  Unlocks the read lock on the global SoDB mutex.

  \sa SoDB::readlock()
  \since Coin 2.3
  \since TGS Inventor 3.0
*/
void
SoDB::readunlock(void)
{
#ifdef OBOL_THREADSAFE
  SoDBP::globalmutex->readUnlock();
#endif // OBOL_THREADSAFE
}

/*!
  Places a write lock on the global SoDB mutex. This can be used to
  prevent that the scene graph is read or traversed while you modify
  the scene graph.

  If you call this function, you must make sure that you also call
  SoDB::writeunlock(). If you fail to do this, you might experience
  that your application locks up.

  \sa SoDB::readlock()
  \since Coin 2.3
  \since TGS Inventor 3.0
*/
void
SoDB::writelock(void)
{
#ifdef OBOL_THREADSAFE
  SoDBP::globalmutex->writeLock();
#endif // OBOL_THREADSAFE
}

/*!
  Unlocks the write lock on the global SoDB mutex.

  \sa SoDB::writelock()
  \since Coin 2.3
  \since TGS Inventor 3.0
*/
void
SoDB::writeunlock(void)
{
#ifdef OBOL_THREADSAFE
  SoDBP::globalmutex->writeUnlock();
#endif // OBOL_THREADSAFE
}

// *************************************************************************

//
// helper function for createRoute(). First test the actual fieldname,
// then set set_<fieldname>, then <fieldname>_changed.
//
static SoField *
find_route_field(SoNode * node, const SbName & fieldname)
{
  SoField * field = node->getField(fieldname);

  if (!field) {
    if (strncmp(fieldname.getString(), "set_", 4) == 0) {
      SbName newname = fieldname.getString() + 4;
      field = node->getField(newname);
    }
    else {
      SbString str = fieldname.getString();
      int len = str.getLength();
      const char CHANGED[] = "_changed";
      const int changedsize = sizeof(CHANGED) - 1;

      if (len > changedsize && strcmp(str.getString()+len-changedsize,
                                      CHANGED) == 0) {
        SbString substr = str.getSubString(0, len-(changedsize+1));
        SbName newname = substr.getString();
        field = node->getField(newname);
      }
    }
  }
  return field;
}

/*!
  Create a connection from one node field to another.

  ("Routes" are what field-to-field connections are called for the
  Inventor standard.)

  Connections made in this manner will be persistent upon file export.

  \sa SoDB::removeRoute()
  \sa SoField::connectFrom(SoField*)

  \since Coin 2.4
  \since TGS Inventor 2.6
*/
void
SoDB::createRoute(SoNode * fromnode, const char * eventout,
                  SoNode * tonode, const char * eventin)
{
  assert(fromnode && tonode && eventout && eventin);

  SbName fromfieldname(eventout);
  SbName tofieldname(eventin);

  SoField * from = find_route_field(fromnode, fromfieldname);
  SoField * to = find_route_field(tonode, tofieldname);

  SbName fromnodename = fromnode->getName();
  if (fromnodename == "") {
    fromnodename = "<noname>";
  }
  SbName tonodename = tonode->getName();
  if (tonodename == "") {
    tonodename = "<noname>";
  }
  SoEngineOutput * output = NULL;
  if (from == NULL && fromnode->isOfType(SoNodeEngine::getClassTypeId())) {
    output = ((SoNodeEngine*) fromnode)->getOutput(fromfieldname);
  }

  if (to && (from || output)) {
    SbBool notnotify = FALSE;
    SbBool append = FALSE;
    if (output || from->getFieldType() == SoField::EVENTOUT_FIELD) {
      notnotify = TRUE;
    }
    append = TRUE;

    // Check if we're already connected.
    SoFieldList fl;
    if (from) from->getForwardConnections(fl);
    else output->getForwardConnections(fl);
    int idx = fl.find(to);
    if (idx != -1) {
#if OBOL_DEBUG
      SoDebugError::postWarning("SoDB::createRoute",
                                "Tried to connect a ROUTE multiple times "
                                "(from %s.%s to %s.%s)",
                                fromnodename.getString(), fromfieldname.getString(),
                                tonodename.getString(), tofieldname.getString());
#endif // OBOL_DEBUG
      return;
    }

    // Check that there exists a field converter, if one is needed.
    SoType totype = to->getTypeId();
    SoType fromtype = from ? from->getTypeId() : output->getConnectionType();
    if (totype != fromtype) {
      SoType convtype = SoDB::getConverter(fromtype, totype);
      if (convtype == SoType::badType()) {
#if OBOL_DEBUG
        SoDebugError::postWarning("SoDB::createRoute",
                                  "Tried to connect a ROUTE between entities "
                                  "that cannot be connected (due to lack of "
                                  "field type converter): %s.%s is of type "
                                  "%s, and %s.%s is of type %s",
                                  fromnodename.getString(), fromfieldname.getString(),
                                  fromtype.getName().getString(),
                                  tonodename.getString(), tofieldname.getString(),
                                  totype.getName().getString());
#endif // OBOL_DEBUG
        return;
      }
    }

    [[maybe_unused]] SbBool ok;
    if (from) ok = to->connectFrom(from, notnotify, append);
    else ok = to->connectFrom(output, notnotify, append);
    // Both known possible failure points are caught above.
    assert(ok && "unexpected connection error");
  }
#if OBOL_DEBUG
  else {
    SoDebugError::postWarning("SoDB::createRoute",
                              "Unable to create route: %s.%s TO %s.%s",
                              fromnodename.getString(), eventout,
                              tonodename.getString(), eventin);
  }
#endif  // OBOL_DEBUG
}

/*!
  Removes a field-to-field connection.

  \sa SoDB::createRoute()
  \sa SoField::disconnect(SoField*)

  \since Coin 2.4
  \since TGS Inventor 2.6
*/
void
SoDB::removeRoute(SoNode * fromnode, const char * eventout,
                  SoNode * tonode, const char * eventin)
{
  assert(fromnode && tonode && eventout && eventin);

  SbName fromfieldname(eventout);
  SbName tofieldname(eventin);

  SoField * from = find_route_field(fromnode, fromfieldname);
  SoField * to = find_route_field(tonode, tofieldname);

  SoEngineOutput * output = NULL;
  if (from == NULL && fromnode->isOfType(SoNodeEngine::getClassTypeId())) {
    output = ((SoNodeEngine*) fromnode)->getOutput(fromfieldname);
  }

  SbName fromnodename = fromnode->getName();
  if (fromnodename == "") {
    fromnodename = "<noname>";
  }
  SbName tonodename = tonode->getName();
  if (tonodename == "") {
    tonodename = "<noname>";
  }

  if (to && (from || output)) {
    if (from) to->disconnect(from);
    else to->disconnect(output);
  }
#if OBOL_DEBUG
  else { // some error occurred
    SoDebugError::postWarning("SoDB::removeRoute",
                              "Unable to remove route: %s.%s TO %s.%s",
                              fromnodename.getString(), eventout,
                              tonodename.getString(), eventin);
  }
#endif // OBOL_DEBUG
}

// *************************************************************************

// Context manager support for applications
// This provides a public API for applications to register OpenGL context
// management before SoDB::init(), which is essential for proper
// initialization ordering with custom rendering backends.

/*!
  Get the currently set global context manager.
  
  \return The current context manager, or NULL if none is set
  
  \since Coin 4.0
*/
SoDB::ContextManager *
SoDB::getContextManager(void)
{
  return global_context_manager;
}

/*!
  Replace the active context manager at runtime without triggering a full
  re-initialisation of the library.  The caller is responsible for ensuring
  that no render is in progress when this is called.  Passing NULL is a no-op.

  \since Coin 4.0
*/
void
SoDB::setContextManager(ContextManager * manager)
{
  if (manager) {
    global_context_manager = manager;
  }
}

/* -----------------------------------------------------------------------
 * SoDB::createOSMesaContextManager() factory
 *
 * The actual implementation lives in SoDBOSMesa.cpp which is compiled with
 * the OSMesa include paths.  In OSMesa-capable builds that TU exports the
 * C helper coin_create_osmesa_context_manager_impl(); we forward to it.
 * In builds without OSMesa support the function returns nullptr.
 * --------------------------------------------------------------------- */
#if defined(OBOL_OSMESA_BUILD) || defined(OBOL_BUILD_DUAL_GL)
extern "C" SoDB::ContextManager * coin_create_osmesa_context_manager_impl();
#endif

SoDB::ContextManager *
SoDB::createOSMesaContextManager()
{
#if defined(OBOL_OSMESA_BUILD) || defined(OBOL_BUILD_DUAL_GL)
  return coin_create_osmesa_context_manager_impl();
#else
  return nullptr;
#endif
}

/* *********************************************************************** */

