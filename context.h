#ifndef __GPHOTOFS2_CONTEXT_H_
#define __GPHOTOFS2_CONTEXT_H_

#include <string>
#include <gphoto2/gphoto2.h>
#include <fuse.h>
#include <mutex>

#include "dir.h"

class Context {
public:
    Context();
    ~Context();
    Camera *camera() { return camera_; }
    GPContext *context() { return context_; }
    uid_t uid() { return uid_; }
    gid_t gid() { return gid_; }
    Dir& root() { return root_; }
    struct statvfs *statCache() { return statCache_; }
    std::mutex& lock() { return lock_; }
    void cacheStat(struct statvfs *newStat) {
        if (statCache_ == nullptr) statCache_ = new struct statvfs();
        *statCache_ = *newStat;
    }

private:
    Camera *camera_;
    GPContext *context_;
    CameraAbilitiesList *abilities_;
    int debug_func_id_;

    uid_t uid_;
    gid_t gid_;

    std::string directory_;
    Dir root_;
    struct statvfs *statCache_;
    // giant lock!
    std::mutex lock_;
};


#endif // __GPHOTOFS2_CONTEXT_H_
