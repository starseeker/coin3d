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
  \class SbImage SbImage.h Inventor/SbImage.h
  \brief The SbImage class is an abstract data type for 2D and 3D images.

  \ingroup coin_base

  \OBOL_CLASS_EXTENSION
  \since Coin 1.0
*/

// FIXME: this class could be used to handle image reusage, since it's
// quite common that the same image is used several times in a scene
// and for different contexts. The API should stay the same though.
// 20001026 mortene (original comment by pederb).

/*!
  \typedef SbBool SbImageScheduleReadCB(const SbString &, SbImage *, void *)

  The type definition of the callback function that is called when a file is
  scheduled for reading.
*/

/*!
  \typedef SbBool SbImageReadImageCB(const SbString &, SbImage *, void *)

  The type definition of the callback function that is called to actually
  read the image file.
*/

#include <Inventor/SbImage.h>

#include <cstring>
#include <algorithm>
#include <condition_variable>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <vector>

#include <Inventor/SbVec2s.h>
#include <Inventor/SbVec3s.h>
#include <Inventor/SbString.h>
#include <Inventor/SoInput.h> // for SoInput::searchForFile()
#include <Inventor/lists/SbStringList.h>
#include <Inventor/lists/SbList.h>
#include <Inventor/errors/SoDebugError.h>
#include "CoinTidbits.h"

#include "config.h"

#include <Inventor/threads/SbRWMutex.h>

#include "SbImageFormatHandler.h"

#ifndef OBOL_WORKAROUND_NO_USING_STD_FUNCS
using std::memcmp;
using std::memcpy;
#endif // !OBOL_WORKAROUND_NO_USING_STD_FUNCS

class SbImageP {
public:
  typedef struct {
    SbImageReadImageCB * cb;
    void * closure;
  } ReadImageCBData;

  enum DataType {
    INTERNAL_DATA,
    FORMAT_HANDLER_DATA,
    SETVALUEPTR_DATA
  };

  SbImageP(void)
    : bytes(NULL),
      datatype(SETVALUEPTR_DATA),
      size(0,0,0),
      bpp(0),
      schedulecb(NULL),
      scheduleclosure(NULL),
      scheduleloading(false)
    , rwmutex(SbRWMutex::READ_PRECEDENCE)
  { }
  void freeData(void) {
    if (this->bytes) {
      switch (this->datatype) {
      default:
        assert(0 && "unknown data type");
        break;
      case INTERNAL_DATA:
        delete[] this->bytes;
        this->bytes = NULL;
        break;
      case FORMAT_HANDLER_DATA:
        SbImageFormatRegistry::getInstance().freeImageData(this->bytes);
        this->bytes = NULL;
        break;
      case SETVALUEPTR_DATA:
        this->bytes = NULL;
        break;
      }
    }
    this->datatype = SETVALUEPTR_DATA;
  }

  unsigned char * bytes;
  DataType datatype;
  SbVec3s size;
  int bpp;
  SbString schedulename;
  SbImageScheduleReadCB * schedulecb;
  void * scheduleclosure;
  std::mutex schedulemutex;
  std::condition_variable schedulecondition;
  bool scheduleloading;

  SbRWMutex rwmutex;
  void readLock(void) {
    //    fprintf(stderr,"readlock: %p\n", this);
    this->rwmutex.readLock();
    //fprintf(stderr,"readlock achieved: %p\n", this);
  }
  void readUnlock(void) {
    //fprintf(stderr,"readUnlock: %p\n", this);
    this->rwmutex.readUnlock();
  }
  void writeLock(void) {
    //fprintf(stderr,"writelock: %p\n", this);
    this->rwmutex.writeLock();
    //fprintf(stderr,"writelock achieved: %p\n", this);
  }
  void writeUnlock(void) {
    //fprintf(stderr,"writeUnlock: %p\n", this);
    this->rwmutex.writeUnlock();
  }

};

namespace {

class SbImageReadGuard {
public:
  explicit SbImageReadGuard(SbImageP * image) : image_(image)
  {
    this->image_->readLock();
  }
  ~SbImageReadGuard() { this->image_->readUnlock(); }
  SbImageReadGuard(const SbImageReadGuard &) = delete;
  SbImageReadGuard & operator=(const SbImageReadGuard &) = delete;

private:
  SbImageP * image_;
};

class SbImageWriteGuard {
public:
  explicit SbImageWriteGuard(SbImageP * image) : image_(image)
  {
    this->image_->writeLock();
  }
  ~SbImageWriteGuard() { this->image_->writeUnlock(); }
  SbImageWriteGuard(const SbImageWriteGuard &) = delete;
  SbImageWriteGuard & operator=(const SbImageWriteGuard &) = delete;

private:
  SbImageP * image_;
};

struct SbImageReadCallbackRegistry {
  std::mutex mutex;
  std::vector<SbImageP::ReadImageCBData> callbacks;
};

SbImageReadCallbackRegistry &
sbimage_read_callback_registry()
{
  // The registry intentionally has process lifetime. This avoids shutdown
  // ordering races with image loaders and removes the old std::atexit hook.
  static SbImageReadCallbackRegistry * registry =
    new SbImageReadCallbackRegistry;
  return *registry;
}

} // namespace

static bool
checked_image_buffer_size(const SbVec3s & size, int bytesperpixel,
                          size_t & buffersize)
{
  if (size[0] < 0 || size[1] < 0 || size[2] < 0 || bytesperpixel < 0) {
    return false;
  }

  const size_t maxsize = std::numeric_limits<size_t>::max();
  buffersize = 1;
  const size_t dimensions[3] = {
    static_cast<size_t>(size[0]),
    static_cast<size_t>(size[1]),
    static_cast<size_t>(size[2] == 0 ? 1 : size[2])
  };
  for (const size_t dimension : dimensions) {
    if (dimension != 0 && buffersize > maxsize / dimension) return false;
    buffersize *= dimension;
  }
  if (bytesperpixel != 0 &&
      buffersize > maxsize / static_cast<size_t>(bytesperpixel)) {
    return false;
  }
  buffersize *= static_cast<size_t>(bytesperpixel);
  return buffersize <= maxsize - 3;
}

//////////////////////////////////////////////////////////////////////////

#define PRIVATE(image) ((image)->pimpl)

/*!
  Default constructor.
*/
SbImage::SbImage(void)
{
  PRIVATE(this) = new SbImageP;
}

/*!
  Constructor which sets 2D data using setValue().
  \sa setValue()
*/
SbImage::SbImage(const unsigned char * bytes,
                 const SbVec2s & size, const int bytesperpixel)
{
  PRIVATE(this) = new SbImageP;
  this->setValue(size, bytesperpixel, bytes);
}

/*!
  Constructor which sets 3D data using setValue().

  \OBOL_FUNCTION_EXTENSION

  \sa setValue()
  \since Coin 2.0
*/
SbImage::SbImage(const unsigned char * bytes,
                 const SbVec3s & size, const int bytesperpixel)
{
  PRIVATE(this) = new SbImageP;
  this->setValue(size, bytesperpixel, bytes);
}

/*!
  Copy constructor

  \since Coin 4.0
 */
SbImage::SbImage(const SbImage & that)
{
  PRIVATE(this) = new SbImageP;
  *this=that;
}

/*!
  Destructor.
*/
SbImage::~SbImage(void)
{
  PRIVATE(this)->freeData();
  delete PRIVATE(this);
}

/*!
  Apply a read lock on this image. This will make it impossible for
  other threads to change the image while this lock is active. Other
  threads can do read-only operations on this image, of course.

  For the single thread version of Coin, this method does nothing.

  \sa readUnlock()
  \since Coin 2.0
*/
void
SbImage::readLock(void) const
{
  PRIVATE(this)->readLock();
}

/*!
  Release a read lock on this image.

  For the single thread version of Coin, this method does nothing.

  \sa readLock()
  \since Coin 2.0
*/
void
SbImage::readUnlock(void) const
{
  PRIVATE(this)->readUnlock();
}

/*!
  Convenience 2D version of setValuePtr.

  \sa setValue()
  \since Coin 2.0
*/
void
SbImage::setValuePtr(const SbVec2s & size, const int bytesperpixel,
                     const unsigned char * bytes)
{
  SbVec3s tmpsize(size[0], size[1], 0);
  this->setValuePtr(tmpsize, bytesperpixel, bytes);
}

/*!
  Sets the image data without copying the data. \a bytes will be used
  directly, and the data will not be freed when the image instance is
  destructed.

  If the depth of the image (size[2]) is zero, the image is considered
  a 2D image.

  \sa setValue()
  \since Coin 2.0
*/
void
SbImage::setValuePtr(const SbVec3s & size, const int bytesperpixel,
                     const unsigned char * bytes)
{
  size_t ignored_buffersize = 0;
  if (!checked_image_buffer_size(size, bytesperpixel, ignored_buffersize)) {
    SoDebugError::postWarning("SbImage::setValuePtr",
                              "Image dimensions or component count are invalid");
    return;
  }
  const SbImageWriteGuard lock(PRIVATE(this));
  PRIVATE(this)->schedulename = "";
  PRIVATE(this)->schedulecb = NULL;
  PRIVATE(this)->freeData();
  PRIVATE(this)->bytes = const_cast<unsigned char *>(bytes);
  PRIVATE(this)->datatype = SbImageP::SETVALUEPTR_DATA;
  PRIVATE(this)->size = size;
  PRIVATE(this)->bpp = bytesperpixel;
}

/*!
  Convenience 2D version of setValue.
*/
void
SbImage::setValue(const SbVec2s & size, const int bytesperpixel,
                  const unsigned char * bytes)
{
  SbVec3s tmpsize(size[0], size[1], 0);
  this->setValue(tmpsize, bytesperpixel, bytes);
}

/*!
  Sets the image to \a size and \a bytesperpixel. If \a bytes !=
  NULL, data is copied from \a bytes into this class' image data. If
  \a bytes == NULL, the image data is left uninitialized.

  The image data will always be allocated in multiples of four. This
  means that if you set an image with size == (1,1,1) and bytesperpixel
  == 1, four bytes will be allocated to hold the data. This is mainly
  done to simplify the export code in SoSFImage and normally you'll
  not have to worry about this feature.

  If the depth of the image (size[2]) is zero, the image is considered
  a 2D image.

  \since Coin 2.0
*/
void
SbImage::setValue(const SbVec3s & size, const int bytesperpixel,
                  const unsigned char * bytes)
{
  size_t buffersize = 0;
  if (!checked_image_buffer_size(size, bytesperpixel, buffersize)) {
    SoDebugError::postWarning("SbImage::setValue",
                              "Image dimensions or component count are invalid");
    return;
  }

  std::unique_ptr<unsigned char[]> newbytes;
  if (buffersize) {
    const size_t alignedbuffersize = ((buffersize + 3) / 4) * 4;
    newbytes = std::make_unique<unsigned char[]>(alignedbuffersize);
    if (bytes) (void)memcpy(newbytes.get(), bytes, buffersize);
  }

  const SbImageWriteGuard lock(PRIVATE(this));
  PRIVATE(this)->schedulename = "";
  PRIVATE(this)->schedulecb = NULL;
  PRIVATE(this)->freeData();
  PRIVATE(this)->size = size;
  PRIVATE(this)->bpp = bytesperpixel;
  if (newbytes) {
    PRIVATE(this)->bytes = newbytes.release();
    PRIVATE(this)->datatype = SbImageP::INTERNAL_DATA;
  }
}

/*!
  Returns the 2D image data.
*/
unsigned char *
SbImage::getValue(SbVec2s & size, int & bytesperpixel) const
{
  SbVec3s tmpsize;
  unsigned char *bytes = this->getValue(tmpsize, bytesperpixel);
  size.setValue(tmpsize[0], tmpsize[1]);
  return bytes;
}

/*!
  Returns the 3D image data.

  \since Coin 2.0
*/
unsigned char *
SbImage::getValue(SbVec3s & size, int & bytesperpixel) const
{
  SbImageScheduleReadCB * callback = NULL;
  void * closure = NULL;
  SbString schedulename;

  // Only one thread may execute a deferred loader for an image. Other
  // readers wait for that loader to finish instead of observing the empty
  // placeholder that precedes a synchronous setValue() callback.
  std::unique_lock<std::mutex> schedulelock(PRIVATE(this)->schedulemutex);
  PRIVATE(this)->schedulecondition.wait(schedulelock, [this] {
    return !PRIVATE(this)->scheduleloading;
  });

  {
    const SbImageReadGuard lock(PRIVATE(this));
    if (!PRIVATE(this)->schedulecb) {
      size = PRIVATE(this)->size;
      bytesperpixel = PRIVATE(this)->bpp;
      return PRIVATE(this)->bytes;
    }
  }

  // Atomically claim the scheduled callback. User code must never execute
  // while an SbImage lock is held: a synchronous loader commonly calls
  // setValue(), which needs the write side of the same mutex.
  {
    const SbImageWriteGuard lock(PRIVATE(this));
    callback = PRIVATE(this)->schedulecb;
    if (callback) {
      closure = PRIVATE(this)->scheduleclosure;
      schedulename = PRIVATE(this)->schedulename;
      PRIVATE(this)->schedulecb = NULL;
      PRIVATE(this)->scheduleloading = true;
    }
  }

  schedulelock.unlock();

  const auto finishScheduledRead = [this] {
    {
      const std::lock_guard<std::mutex> lock(PRIVATE(this)->schedulemutex);
      PRIVATE(this)->scheduleloading = false;
    }
    PRIVATE(this)->schedulecondition.notify_all();
  };

  if (callback) {
    SbBool scheduled = FALSE;
    try {
      scheduled = callback(schedulename, const_cast<SbImage *>(this), closure);
    }
    catch (...) {
      const SbImageWriteGuard lock(PRIVATE(this));
      if (!PRIVATE(this)->schedulecb &&
          PRIVATE(this)->schedulename == schedulename) {
        PRIVATE(this)->schedulecb = callback;
        PRIVATE(this)->scheduleclosure = closure;
      }
      finishScheduledRead();
      throw;
    }
    if (!scheduled) {
      const SbImageWriteGuard lock(PRIVATE(this));
      if (!PRIVATE(this)->schedulecb &&
          PRIVATE(this)->schedulename == schedulename) {
        PRIVATE(this)->schedulecb = callback;
        PRIVATE(this)->scheduleclosure = closure;
      }
    }
    finishScheduledRead();
  }

  const SbImageReadGuard lock(PRIVATE(this));
  size = PRIVATE(this)->size;
  bytesperpixel = PRIVATE(this)->bpp;
  return PRIVATE(this)->bytes;

}

/*!
  Given a \a basename for a file and and array of directories to
  search (in \a dirlist, of length \a numdirs), returns the full name
  of the file found.

  In addition to looking at the root of each directory in \a dirlist,
  we also look into the subdirectories \e texture/, \e textures/, \e
  images/, \e pics/ and \e pictures/ of each \a dirlist directory.

  If no file matching \a basename could be found, returns an empty
  string.
*/
SbString
SbImage::searchForFile(const SbString & basename,
                       const SbString * const * dirlist, const int numdirs)
{
  int i;
  SbStringList directories;
  SbStringList subdirectories;

  for (i = 0; i < numdirs; i++) {
    directories.append(const_cast<SbString *>(dirlist[i]));
  }
  subdirectories.append(new SbString("texture"));
  subdirectories.append(new SbString("textures"));
  subdirectories.append(new SbString("images"));
  subdirectories.append(new SbString("pics"));
  subdirectories.append(new SbString("pictures"));

  SbString ret = SoInput::searchForFile(basename, directories, subdirectories);
  for (i = 0; i < subdirectories.getLength(); i++) {
    delete subdirectories[i];
  }
  return ret;
}

/*!
  Reads image data from \a filename. In Coin, simage is used to
  load image files, and several common file formats are supported.
  simage can be downloaded from our web pages.  If loading
  fails for some reason this method returns FALSE, and the instance
  is set to an empty image. If the file is successfully loaded, the
  file image data is copied into this class.

  If \a numdirectories > 0, this method will search for \a filename
  in all directories in \a searchdirectories.
*/
SbBool
SbImage::readFile(const SbString & filename,
                  const SbString * const * searchdirectories,
                  const int numdirectories)
{
  // FIXME: Add 3D image support when that is added to format handlers (kintel 20011118)

  if (filename.getLength() == 0) {
    // This is really an internal error, should perhaps assert. <mortene>.
    SoDebugError::post("SbImage::readFile",
                       "attempted to read file from empty filename.");
    return FALSE;
  }

  SbString finalname = SbImage::searchForFile(filename, searchdirectories,
                                              numdirectories);

  std::vector<SbImageP::ReadImageCBData> callbacks;
  {
    SbImageReadCallbackRegistry & registry = sbimage_read_callback_registry();
    const std::lock_guard<std::mutex> lock(registry.mutex);
    callbacks = registry.callbacks;
  }
  for (const SbImageP::ReadImageCBData & cbdata : callbacks) {
    if (finalname.getLength() > 0 &&
        cbdata.cb(finalname, this, cbdata.closure)) return TRUE;
    if (cbdata.cb(filename, this, cbdata.closure)) return TRUE;
  }

  if (finalname.getLength() == 0) {
    SoDebugError::post("SbImage::readFile",
                       "couldn't find '%s'.", filename.getString());
    return FALSE;
  }
  
  // try format handlers
  auto& registry = SbImageFormatRegistry::getInstance();
  int w, h, nc;
  unsigned char * imagedata = registry.readImage(finalname.getString(), &w, &h, &nc);
  
  if (imagedata) {
    if (w < 0 || h < 0 ||
        w > std::numeric_limits<short>::max() ||
        h > std::numeric_limits<short>::max()) {
      registry.freeImageData(imagedata);
      SoDebugError::post("SbImage::readFile",
                         "image dimensions exceed SbImage limits");
      return FALSE;
    }
    const SbImageWriteGuard lock(PRIVATE(this));
    PRIVATE(this)->schedulename = "";
    PRIVATE(this)->schedulecb = NULL;
    PRIVATE(this)->freeData();
    PRIVATE(this)->bytes = imagedata;
    PRIVATE(this)->datatype = SbImageP::FORMAT_HANDLER_DATA;
    PRIVATE(this)->size.setValue(static_cast<short>(w),
                                 static_cast<short>(h), 0);
    PRIVATE(this)->bpp = nc;
    return TRUE;
  }
#if OBOL_DEBUG
  else {
    SoDebugError::post("SbImage::readFile", "(%s) %s",
                       filename.getString(),
                       registry.getLastError());
  }
#endif // OBOL_DEBUG
    
  this->setValue(SbVec3s(0,0,0), 0, NULL);
  return FALSE;
}

/*!
  \fn int SbImage::operator!=(const SbImage & image) const
  Compare image of \a image with the image in this class and
  return \c FALSE if they are equal.
*/


/*!
  Compare image of \a image with the image in this class and
  return \c TRUE if they are equal.
*/
int
SbImage::operator==(const SbImage & image) const
{
  if (this == &image) return TRUE;

  SbImageP * first = PRIVATE(this);
  SbImageP * second = PRIVATE(&image);
  if (std::less<SbImageP *>()(second, first)) std::swap(first, second);
  const SbImageReadGuard firstlock(first);
  const SbImageReadGuard secondlock(second);

  int ret = 0;
  if (!PRIVATE(this)->schedulecb && !PRIVATE(&image)->schedulecb) {
    if (PRIVATE(this)->size != PRIVATE(&image)->size) ret = 0;
    else if (PRIVATE(this)->bpp != PRIVATE(&image)->bpp) ret = 0;
    else if (PRIVATE(this)->bytes == NULL || PRIVATE(&image)->bytes == NULL) {
      ret = (PRIVATE(this)->bytes == PRIVATE(&image)->bytes);
    }
    else {
      size_t buffersize = size_t(PRIVATE(this)->size[0]) *
          size_t(PRIVATE(this)->size[1]) *
          size_t(PRIVATE(this)->size[2] == 0 ? 1 : PRIVATE(this)->size[2]) *
          size_t(PRIVATE(this)->bpp);
      ret = memcmp(PRIVATE(this)->bytes, PRIVATE(&image)->bytes,
                   buffersize) == 0;
    }
  }
  return ret;
}

/*!
  Assignment operator.
*/
SbImage & 
SbImage::operator=(const SbImage & image)
{
  if (this == &image) return *this;

  SbVec3s size;
  int bpp = 0;
  SbImageP::DataType datatype = SbImageP::SETVALUEPTR_DATA;
  unsigned char * externalbytes = NULL;
  std::unique_ptr<unsigned char[]> ownedbytes;
  SbString schedulename;
  SbImageScheduleReadCB * schedulecb = NULL;
  void * scheduleclosure = NULL;

  {
    const SbImageReadGuard lock(PRIVATE(&image));
    size = PRIVATE(&image)->size;
    bpp = PRIVATE(&image)->bpp;
    datatype = PRIVATE(&image)->datatype;
    schedulename = PRIVATE(&image)->schedulename;
    schedulecb = PRIVATE(&image)->schedulecb;
    scheduleclosure = PRIVATE(&image)->scheduleclosure;

    if (PRIVATE(&image)->bytes &&
        datatype != SbImageP::SETVALUEPTR_DATA) {
      size_t buffersize = 0;
      if (!checked_image_buffer_size(size, bpp, buffersize)) {
        throw std::length_error("invalid SbImage source dimensions");
      }
      const size_t alignedbuffersize = ((buffersize + 3) / 4) * 4;
      ownedbytes = std::make_unique<unsigned char[]>(alignedbuffersize);
      (void)memcpy(ownedbytes.get(), PRIVATE(&image)->bytes, buffersize);
      datatype = SbImageP::INTERNAL_DATA;
    }
    else {
      externalbytes = PRIVATE(&image)->bytes;
    }
  }

  const SbImageWriteGuard lock(PRIVATE(this));
  PRIVATE(this)->freeData();
  PRIVATE(this)->size = size;
  PRIVATE(this)->bpp = bpp;
  PRIVATE(this)->schedulename = schedulename;
  PRIVATE(this)->schedulecb = schedulecb;
  PRIVATE(this)->scheduleclosure = scheduleclosure;
  if (ownedbytes) {
    PRIVATE(this)->bytes = ownedbytes.release();
    PRIVATE(this)->datatype = SbImageP::INTERNAL_DATA;
  }
  else {
    PRIVATE(this)->bytes = externalbytes;
    PRIVATE(this)->datatype = datatype;
  }
  return *this;
}


/*!
  Schedule a file for reading. \a cb will be called the first time
  getValue() is called for this image. The callback is invoked without
  holding the image mutex, so it may either start an asynchronous read or
  synchronously populate the image with setValue(). Only one thread invokes a
  pending callback; concurrent getValue() callers wait for that invocation to
  return. The callback must therefore not recursively call getValue() on the
  same image. The caller must keep a closure alive until the scheduled read
  has run or the image has been destroyed.

  \sa readFile()
  \since Coin 2.0
*/
SbBool
SbImage::scheduleReadFile(SbImageScheduleReadCB * cb,
                          void * closure,
                          const SbString & filename,
                          const SbString * const * searchdirectories,
                          const int numdirectories)
{
  const SbString resolvedname =
    this->searchForFile(filename, searchdirectories, numdirectories);
  this->setValue(SbVec3s(0,0,0), 0, NULL);
  const SbImageWriteGuard lock(PRIVATE(this));
  PRIVATE(this)->schedulecb = NULL;
  PRIVATE(this)->schedulename = resolvedname;
  int len = PRIVATE(this)->schedulename.getLength();
  if (len > 0) {
    PRIVATE(this)->schedulecb = cb;
    PRIVATE(this)->scheduleclosure = closure;
  }
  return len > 0;
}

/*!
  Returns \a TRUE if the image is not empty. This can be useful, since
  getValue() will start loading the image if scheduleReadFile() has
  been used to set the image data.

  \since Coin 2.0
*/
SbBool 
SbImage::hasData(void) const
{
  const SbImageReadGuard lock(PRIVATE(this));
  return PRIVATE(this)->bytes != NULL;
}

/*!
  Returns the size of the image. If this is a 2D image, the
  z component is zero. If this is a 3D image, the z component is
  >= 1.

  \since Coin 2.0
 */
SbVec3s
SbImage::getSize(void) const
{
  const SbImageReadGuard lock(PRIVATE(this));
  return PRIVATE(this)->size;
}

/*!
  Add a callback which will be called whenever Coin wants to read an
  image file.  The callback should return TRUE if it was able to
  successfully read and set the image data, and FALSE otherwise.

  The callback(s) will be called before attempting to use simage to
  load images. Registry dispatch uses a snapshot and does not hold the
  registry mutex while client code runs. Consequently, callbacks may add or
  remove callbacks reentrantly, but a removal does not cancel an invocation
  that was already present in an active snapshot. Keep a closure alive until
  all such reads have completed.
  
  \sa removeReadImageCB()
  \since Coin 3.0
*/
void 
SbImage::addReadImageCB(SbImageReadImageCB * cb, void * closure)
{
  if (!cb) return;
  SbImageReadCallbackRegistry & registry = sbimage_read_callback_registry();
  const std::lock_guard<std::mutex> lock(registry.mutex);
  registry.callbacks.push_back({ cb, closure });
}

/*!
  Remove a read image callback added with addReadImageCB(). This prevents the
  callback from appearing in future dispatch snapshots; it does not wait for
  invocations from snapshots already in progress.

  \sa addReadImageCB()
  \since Coin 3.0
*/
void 
SbImage::removeReadImageCB(SbImageReadImageCB * cb, void * closure)
{
  SbImageReadCallbackRegistry & registry = sbimage_read_callback_registry();
  const std::lock_guard<std::mutex> lock(registry.mutex);
  const auto iter = std::find_if(
    registry.callbacks.begin(), registry.callbacks.end(),
    [cb, closure](const SbImageP::ReadImageCBData & data) {
      return data.cb == cb && data.closure == closure;
    });
  if (iter != registry.callbacks.end()) registry.callbacks.erase(iter);
}

#undef PRIVATE
