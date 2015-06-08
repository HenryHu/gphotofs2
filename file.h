#ifndef __GPHOTOFS2_FILE_H_
#define __GPHOTOFS2_FILE_H_

#include <string>
#include <gphoto2/gphoto2.h>
#include <mutex>

#include "utils.h"

struct File {
    std::string name;
    CameraFile *camFile;
    char *buf;
    off_t size;
//    bool writeable;
    int mtime;
    int ref;
    bool changed;
    std::mutex lock;

    File(const std::string& name, const CameraFileInfo& info) {
        this->name = name;
        mtime = info.file.mtime;
        size = info.file.size;
        buf = NULL;
        ref = 0;
        changed = false;
        camFile = nullptr;
    }

    File(const std::string& name) {
        this->name = name;
        mtime = Now();
        size = 0;
        buf = NULL;
        ref = 0;
        changed = false;
        camFile = nullptr;
    }

    ~File() {
        if (ref > 0) {
            Error("deleting active file");
        }
        if (buf) delete[] buf;
        if (camFile) gp_file_unref(camFile);
    }
};


#endif // __GPHOTOFS2_FILE_H_
