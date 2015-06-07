#include "utils.h"
#include <gphoto2/gphoto2.h>
#include <iostream>
#include <sys/time.h>
using namespace std;

int Now() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec;
}

void Error(const string& msg) {
    cerr << msg << endl;
}

void Warn(const string& msg) {
    cerr << msg << endl;
}

void Debug(const string& msg) {
    cerr << msg << endl;
}

off_t SizeToBlocks(off_t size) {
    return size / 512 + (size % 512 ? 1 : 0);
}

int gpresultToErrno(int result) {
   switch (result) {
   case GP_ERROR:
      return -EPROTO;
   case GP_ERROR_BAD_PARAMETERS:
      return -EINVAL;
   case GP_ERROR_NO_MEMORY:
      return -ENOMEM;
   case GP_ERROR_LIBRARY:
      return -ENOSYS;
   case GP_ERROR_UNKNOWN_PORT:
      return -ENXIO;
   case GP_ERROR_NOT_SUPPORTED:
      return -EPROTONOSUPPORT;
   case GP_ERROR_TIMEOUT:
      return -ETIMEDOUT;
   case GP_ERROR_IO:
   case GP_ERROR_IO_SUPPORTED_SERIAL:
   case GP_ERROR_IO_SUPPORTED_USB:
   case GP_ERROR_IO_INIT:
   case GP_ERROR_IO_READ:
   case GP_ERROR_IO_WRITE:
   case GP_ERROR_IO_UPDATE:
   case GP_ERROR_IO_SERIAL_SPEED:
   case GP_ERROR_IO_USB_CLEAR_HALT:
   case GP_ERROR_IO_USB_FIND:
   case GP_ERROR_IO_USB_CLAIM:
   case GP_ERROR_IO_LOCK:
      return -EIO;

   case GP_ERROR_CAMERA_BUSY:
      return -EBUSY;
   case GP_ERROR_FILE_NOT_FOUND:
   case GP_ERROR_DIRECTORY_NOT_FOUND:
      return -ENOENT;
   case GP_ERROR_FILE_EXISTS:
   case GP_ERROR_DIRECTORY_EXISTS:
      return -EEXIST;
   case GP_ERROR_PATH_NOT_ABSOLUTE:
      return -ENOTDIR;
   case GP_ERROR_CORRUPTED_DATA:
      return -EIO;
   case GP_ERROR_CANCEL:
      return -ECANCELED;

   /* These are pretty dubious mappings. */
   case GP_ERROR_MODEL_NOT_FOUND:
      return -EPROTO;
   case GP_ERROR_CAMERA_ERROR:
      return -EPERM;
   case GP_ERROR_OS_FAILURE:
      return -EPIPE;
   }
   return -EINVAL;
}


