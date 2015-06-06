#include <string>

#include <fuse.h>
#include <gphoto2/gphoto2.h>
#include <mutex>
#include <vector>
#include <memory>
#include <map>
#include <iostream>

using namespace std;


struct Options {
    string port;
    string model;
    string usbid;
    int speed;
};

struct File {
    string name;
    CameraFile *file;
    void *buf;
    off_t size;
    bool writeable;
    string destdir;
    string destname;
    int mtime;

    File(const string& name, const CameraFileInfo& info) {
        this->name = name;
        mtime = info.file.mtime;
        size = info.file.size;
    }
};

struct Dir {
    string name;

    bool listed;
    map<string, File*> files;
    map<string, Dir*> dirs;

    Dir(const string& name) : name(name), listed(false) {}

    void addFile(File *file) {
        files[file->name] = file;
    }

    void addDir(Dir *dir) {
        dirs[dir->name] = dir;
    }

    File* getFile(const string& name) {
        auto it = files.find(name);
        if (it == files.end()) return nullptr;
        return it->second;
    }

    Dir* getDir(const string& name) {
        auto it = dirs.find(name);
        if (it == dirs.end()) return nullptr;
        return it->second;
    }
};

class Context {
public:
    Context();
    ~Context();
    Camera *camera() { return camera_; }
    GPContext *context() { return context_; }
    uid_t uid() { return uid_; }
    gid_t gid() { return gid_; }
    Dir& root() { return root_; }

private:
    Camera *camera_;
    GPContext *context_;
    CameraAbilitiesList *abilities_;
    int debug_func_id_;

    uid_t uid_;
    gid_t gid_;

    string directory_;
    Dir root_;
};

Context::Context() : root_("") {
    context_ = gp_context_new();
    int ret;
    if ((ret = gp_camera_new(&camera_)) != GP_OK) {
        return;
    }
    gp_abilities_list_new(&abilities_);
    gp_abilities_list_load(abilities_, context_);
    uid_ = getuid();
    gid_ = getgid();
}

Context::~Context() {
    if (abilities_) gp_abilities_list_free(abilities_);
    if (camera_) gp_camera_unref(camera_);
    if (context_) gp_context_unref(context_);
}

Dir* FindDir(const string& path, Context *ctx);
File* FindFile(const string& path, Context *ctx);

void Warn(const string& msg) {
    cerr << msg << endl;
}

void Debug(const string& msg) {
    cerr << msg << endl;
}

static off_t SizeToBlocks(off_t size) {
    return size / 512 + (size % 512 ? 1 : 0);
}

static int
gpresultToErrno(int result)
{
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

/* 
 * Operations
 */

static int Getattr(const char *path, struct stat *st) {
    Context *ctx = (Context*)fuse_get_context()->private_data;

    st->st_uid = ctx->uid();
    st->st_gid = ctx->gid();

    if (strcmp(path, "/") == 0) {
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = 2;
        return 0;
    }

    Dir *dir = FindDir(path, ctx);
    if (dir != nullptr) {
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = 2;
        return 0;
    }

    File *file = FindFile(path, ctx);
    if (file != nullptr) {
        st->st_mode = S_IFREG | 0644;
        st->st_nlink = 1;
        st->st_size = file->size;
        st->st_blocks = SizeToBlocks(file->size);
        st->st_mtime = file->mtime;
        return 0;
    }
    return -ENOENT;
}

/*
 * File ops
 */
File* FindFile(const string& path, Context *ctx) {
    size_t pos = path.rfind("/");
    Dir *dir;
    string name;
    if (pos == string::npos) {
        Warn("path has no /");
        dir = &ctx->root();
        name = path;
    } else {
        string parentPath = path.substr(0, pos + 1);
        dir = FindDir(parentPath, ctx);
        if (dir == nullptr) {
            return nullptr;
        }
        name = path.substr(pos + 1);
    }
    return dir->getFile(name);
}

/*
 * Dir ops
 */

Dir* FindDir(const string& path, Context *ctx) {
    Dir* dir = &ctx->root();
    Debug("finddir: " + path);

    if (path == "/") {
        return dir;
    }

    int last;
    if (path[0] == '/') {
        last = 0;
    } else {
        last = -1;
    }

    while (true) {
        int next = path.find('/', last + 1);
        if (next == string::npos) {
            string name = path.substr(last + 1);
            if (name == "") {
                Debug("return self dir: " + dir->name);
                return dir;
            }
            return dir->getDir(name);
        }
        // next != npos: next >= last + 1
        string name = path.substr(last + 1, next - last - 1);
        Debug("child: " + name);
        if (name == "") {
            last = next;
            continue;
        }
        dir = dir->getDir(name);
        if (dir == nullptr) {
            return nullptr;
        }
        last = next;
    }
}

static int Readdir(const char *path, void *buf, fuse_fill_dir_t filler,
        off_t offset, struct fuse_file_info *fileInfo) {
    Context *ctx = (Context *)fuse_get_context()->private_data;
    Dir *dir = FindDir(path, ctx);
    if (dir == nullptr) {
        return -ENOENT;
    }
    dir->listed = true;

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    CameraList *list = NULL;
    gp_list_new(&list);

    int ret = gp_camera_folder_list_folders(ctx->camera(),
            path, list, ctx->context());
    if (ret != 0) {
        gp_list_free(list);
        return gpresultToErrno(ret);
    }

    for (int i = 0; i < gp_list_count(list); i++) {
        struct stat *st;
        const char *name;

        st = new struct stat();
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = 2;
        st->st_uid = ctx->uid();
        st->st_gid = ctx->gid();

        gp_list_get_name(list, i, &name);
        filler(buf, name, st, 0);
        
        unique_ptr<Dir> subDir(new Dir(name));
        dir->addDir(subDir.release());
        Debug(string("child dir : ") + name + " (" + path + ")");
    }

    gp_list_free(list);
    list = NULL;

    gp_list_new(&list);

    ret = gp_camera_folder_list_files(ctx->camera(),
            path, list, ctx->context());
    if (ret != GP_OK) {
        gp_list_free(list);
        return gpresultToErrno(ret);
    }

    for (int i = 0; i < gp_list_count(list); i++) {
        struct stat *st;
        const char *name;
        CameraFileInfo info;

        gp_list_get_name(list, i, &name);
        ret = gp_camera_file_get_info(ctx->camera(), path, name, &info,
                ctx->context());
        if (ret != GP_OK) {
            gp_list_free(list);
            return gpresultToErrno(ret);
        }

        st = new struct stat();
        st->st_mode = S_IFREG | 0644;
        st->st_nlink = 1;
        st->st_uid = ctx->uid();
        st->st_gid = ctx->gid();
        st->st_size = info.file.size;
        st->st_mtime = info.file.mtime;
        st->st_blocks = (info.file.size / 512) +
            (info.file.size % 512 > 0 ? 1 : 0);

        filler(buf, name, st, 0);

        unique_ptr<File> file(new File(name, info));
        dir->addFile(file.release());
        Debug(string("child file: ") + name + " (" + path + ")");
    }

    gp_list_free(list);
    return 0;
}

/*
 * Meta functions
 */

static void* Init(struct fuse_conn_info *conn) {
    return new Context();
}

static void Destroy(void *void_context) {
    Context *context = (Context *)void_context;
    delete context;
}

static int Statfs(const char *path, struct statvfs *stat) {
    Context *ctx = (Context *)fuse_get_context()->private_data;
    CameraStorageInformation *storageInfo;
    int res, numInfo;

    res = gp_camera_get_storageinfo(ctx->camera(),
            &storageInfo, &numInfo, ctx->context());
    if (res != GP_OK) {
        return gpresultToErrno(res);
    }
    if (numInfo == 0) {
        Warn("num of storage = 0");
        return -EINVAL;
    }
    if (numInfo == 1) {
        stat->f_bsize = 1024;
        stat->f_frsize = 1024;
        stat->f_blocks = storageInfo->capacitykbytes;
        stat->f_bfree = storageInfo->freekbytes;
        stat->f_bavail = storageInfo->freekbytes;
        stat->f_files = -1;
        stat->f_ffree = -1;
    }
    return 0;
}

/*
 * Dummy functions
 */

static int Chmod(const char *path, mode_t mode) {
    return 0;
}

static int Chown(const char *path, uid_t uid, gid_t gid) {
    return 0;
}

static fuse_operations GPhotoFS2_Operations = {
    .init = Init,
    .destroy = Destroy,
    .statfs = Statfs,

    .chown = Chown,
    .chmod = Chmod,

    .getattr = Getattr,

    .readdir = Readdir,
};

int main(int argc, char **argv) {
    setlocale (LC_CTYPE,"en_US.UTF-8"); /* for ptp2 driver to convert to utf-8 */
    fuse_main(argc, argv, &GPhotoFS2_Operations, NULL);
}
