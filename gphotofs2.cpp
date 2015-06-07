#include <string>

#include <fuse.h>
#include <gphoto2/gphoto2.h>
#include <libgen.h>

#include <mutex>
#include <vector>
#include <memory>
#include <map>
#include <iostream>

#include "dir.h"
#include "file.h"
#include "utils.h"
#include "context.h"

using namespace std;

struct Options {
    string port;
    string model;
    string usbid;
    int speed;
};

struct FileDesc {
    bool writeable;
    File *file;
};

static int ListDir(const char *path, Dir *dir, Context *ctx);
Dir* FindDir(const string& path, Context *ctx);
File* FindFile(const string& path, Context *ctx);

/* 
 * Operations
 */

static int Getattr(const char *path, struct stat *st) {
    Context *ctx = (Context*)fuse_get_context()->private_data;

    Dir *dir = FindDir(path, ctx);
    if (dir != nullptr) {
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = 2;
        st->st_uid = ctx->uid();
        st->st_gid = ctx->gid();
        return 0;
    }

    File *file = FindFile(path, ctx);
    if (file != nullptr) {
        st->st_mode = S_IFREG | 0644;
        st->st_nlink = 1;
        st->st_size = file->size;
        st->st_blocks = SizeToBlocks(file->size);
        st->st_mtime = file->mtime;
        st->st_uid = ctx->uid();
        st->st_gid = ctx->gid();
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
        if (!dir->listed) ListDir("/", dir, ctx);
    } else {
        string parentPath = path.substr(0, pos + 1);
        dir = FindDir(parentPath, ctx);
        if (dir == nullptr) {
            return nullptr;
        }
        name = path.substr(pos + 1);
        if (!dir->listed) ListDir(parentPath.c_str(), dir, ctx);
    }
    return dir->getFile(name);
}

static int Create(const char *path, mode_t mode,
        struct fuse_file_info *fileInfo) {
    Context *ctx = (Context *)fuse_get_context()->private_data;

    const char *dirName = dirname(path);
    const char *fileName = basename(path);

    Dir *dir = FindDir(dirName, ctx);
    if (dir == nullptr) {
        Warn("parent dir does not exist");
        return -ENOENT;
    }

    if (dir->getFile(fileName) != nullptr) {
        return -EEXIST;
    }

    File *file = new File(fileName);
    file->changed = true;
    dir->addFile(file);

    if (file->camFile == nullptr) {
        const char *dirName = dirname(path);
        const char *fileName = basename(path);
        CameraFile *camFile;
        gp_file_new(&camFile);
        file->camFile = camFile;
        cerr << "file's camfile init to " << camFile << endl;
    }

    FileDesc *fd = new FileDesc();
    fd->writeable = true;
    fd->file = file;
    file->ref++;
    fileInfo->fh = (uint64_t)fd;
    return 0;
}

static int Open(const char *path, struct fuse_file_info *fileInfo) {
    Context *ctx = (Context *)fuse_get_context()->private_data;
    File *file = FindFile(path, ctx);
    if (file == nullptr) {
        return -ENOENT;
    }
    cerr << "open. file: " << file << endl;
    if (file->camFile == nullptr) {
        const char *dirName = dirname(path);
        const char *fileName = basename(path);
        CameraFile *camFile;
        gp_file_new(&camFile);
        int ret = gp_camera_file_get(ctx->camera(), dirName, fileName,
                GP_FILE_TYPE_NORMAL, camFile, ctx->context());
        if (ret != GP_OK) {
            gp_file_unref(camFile);
            return gpresultToErrno(ret);
        }
        file->camFile = camFile;
        cerr << "file's camfile init to " << camFile << endl;
    }
    FileDesc *fd = new FileDesc();
    int mode = fileInfo->flags & 3;
    if (mode == O_RDONLY) {
        fd->writeable = false;
    } else {
        fd->writeable = true;
    }
    fd->file = file;
    file->ref++;
    fileInfo->fh = (uint64_t)fd;
    return 0;
}

static int Release(const char *path, struct fuse_file_info *fileInfo) {
    Context *ctx = (Context *)fuse_get_context()->private_data;
    FileDesc *fd = (FileDesc *)fileInfo->fh;
    File *file = fd->file;
    delete fd;
    file->ref--;

    if (file->changed) {
        if (file->buf == nullptr) {
            Error("buf freed while still changed!");
            return 0;
        }

        int ret = gp_file_set_data_and_size(file->camFile, file->buf, file->size);
        if (ret != GP_OK) {
            return gpresultToErrno(ret);
        }

        const char *dirName = dirname(path);
        const char *fileName = basename(path);
        ret = gp_camera_file_delete(ctx->camera(), dirName, fileName,
                ctx->context());
        if (ret != GP_OK) {
            Warn(string("fail to delete ") + path);
        }
        ret = gp_camera_folder_put_file(ctx->camera(), dirName, fileName,
                GP_FILE_TYPE_NORMAL, file->camFile, ctx->context());
        if (ret != GP_OK) {
            return gpresultToErrno(ret);
        }
        file->buf = nullptr;
        file->changed = false;
    }

    if (file->ref == 0) {
        gp_file_unref(file->camFile);
        file->camFile = nullptr;
    }
    return 0;
}

static int ReadWholeFile(File *file) {
    const char *data;
    unsigned long dataSize;
    cerr << "camFile: " << file->camFile << endl;
    int ret = gp_file_get_data_and_size(file->camFile, &data, &dataSize);
    if (ret == GP_OK) {
        file->buf = new char[dataSize];
        file->size = dataSize;
        file->changed = false;
        memcpy(file->buf, data, dataSize);
        return 0;
    } else {
        return gpresultToErrno(ret);
    }
}

static int Read(const char *path, char *buf, size_t size, off_t offset,
        struct fuse_file_info *fileInfo) {
    Context *ctx = (Context *)fuse_get_context()->private_data;

    FileDesc *fd = (FileDesc *)fileInfo->fh;
    File *file = fd->file;

    cerr << "file: " << file << endl;
    if (file->buf == nullptr) {
        int ret = ReadWholeFile(file);
        if (ret != 0) return ret;
    }

    if (offset < file->size) {
        if (offset + size > file->size) {
            size = file->size - offset;
        }
        memcpy(buf, file->buf + offset, size);
        return size;
    } else {
        return 0;
    }
}

static int Write(const char *path, const char *buf, size_t size, off_t offset,
        struct fuse_file_info *fileInfo) {
    Context *ctx = (Context *)fuse_get_context()->private_data;

    FileDesc *fd = (FileDesc *)fileInfo->fh;
    File *file = fd->file;

    if (file->buf == nullptr) {
        int ret = ReadWholeFile(file);
        if (ret != 0) return ret;
    }

    if (offset + size > file->size) {
        char *newBuf = (char*)realloc(file->buf, offset + size);
        if (newBuf == nullptr) {
            return -EFBIG;
        }
        file->size = offset + size;
        file->buf = newBuf;
    }
    memcpy(file->buf + offset, buf, size);
    file->changed = true;
    return size;
}

static int Flush(const char *path, struct fuse_file_info *fileInfo) {
    Context *ctx = (Context *)fuse_get_context()->private_data;

    FileDesc *fd = (FileDesc *)fileInfo->fh;
    File *file = fd->file;

    return 0;
}

static int Truncate(const char *path, off_t size) {
    Context *ctx = (Context *)fuse_get_context()->private_data;

    File *file = FindFile(path, ctx);

    if (file->size > size) {
        size = file->size;
    }

    return 0;
}

static int Unlink(const char *path) {
    Context *ctx = (Context *)fuse_get_context()->private_data;
    const char *dirName = dirname(path);
    const char *fileName = basename(path);

    Dir *dir = FindDir(dirName, ctx);
    File *file = FindFile(path, ctx);

    if (dir == nullptr || file == nullptr) {
        return -ENOENT;
    }

    if (file->ref > 0) {
        return -EBUSY;
    }

    int ret = gp_camera_file_delete(ctx->camera(), dirName, fileName,
            ctx->context());
    if (ret != GP_OK) {
        return gpresultToErrno(ret);
    }

    dir->removeFile(file);
    delete file;
    return 0;
}

/*
 * Dir ops
 */

Dir* FindDir(const string& path, Context *ctx) {
    Dir* dir = &ctx->root();
    string dirPath = "/";
    Debug("finddir: " + path);

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
            Debug("last child: " + name);
            if (name == "") {
                Debug("return self dir: " + dir->name);
                return dir;
            }
            if (!dir->listed) {
                ListDir(dirPath.c_str(), dir, ctx);
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
        if (!dir->listed) {
            ListDir(dirPath.c_str(), dir, ctx);
        }
        dir = dir->getDir(name);
        if (dir == nullptr) {
            return nullptr;
        }
        dirPath = dirPath + "/" + name;
        last = next;
    }
}

static int ListDir(const char *path, Dir *dir, Context *ctx) {
    // XXX: Dir should know its path

    CameraList *list = NULL;
    gp_list_new(&list);

    int ret = gp_camera_folder_list_folders(ctx->camera(),
            path, list, ctx->context());
    if (ret != 0) {
        gp_list_free(list);
        return gpresultToErrno(ret);
    }

    for (int i = 0; i < gp_list_count(list); i++) {
        const char *name;
        gp_list_get_name(list, i, &name);
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
        const char *name;
        CameraFileInfo info;

        gp_list_get_name(list, i, &name);
        ret = gp_camera_file_get_info(ctx->camera(), path, name, &info,
                ctx->context());
        if (ret != GP_OK) {
            gp_list_free(list);
            return gpresultToErrno(ret);
        }

        unique_ptr<File> file(new File(name, info));
        dir->addFile(file.release());
        Debug(string("child file: ") + name + " (" + path + ")");
    }

    gp_list_free(list);
    dir->listed = true;
    return 0;
}

static int Readdir(const char *path, void *buf, fuse_fill_dir_t filler,
        off_t offset, struct fuse_file_info *fileInfo) {
    Context *ctx = (Context *)fuse_get_context()->private_data;
    Dir *dir = FindDir(path, ctx);
    if (dir == nullptr) {
        return -ENOENT;
    }
    if (!dir->listed) {
        int ret = ListDir(path, dir, ctx);
        if (ret) return ret;
    }

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    for (auto it = dir->dirs.begin(); it != dir->dirs.end(); it++) {
        Dir *subDir = it->second;

        struct stat st;
        st.st_mode = S_IFDIR | 0755;
        st.st_nlink = 2;
        st.st_uid = ctx->uid();
        st.st_gid = ctx->gid();

        filler(buf, subDir->name.c_str(), &st, 0);
    }

    for (auto it = dir->files.begin(); it != dir->files.end(); it++) {
        File *file = it->second;

        struct stat st;
        st.st_mode = S_IFREG | 0644;
        st.st_nlink = 1;
        st.st_uid = ctx->uid();
        st.st_gid = ctx->gid();
        st.st_size = file->size;
        st.st_mtime = file->mtime;
        st.st_blocks = (file->size / 512) +
            (file->size % 512 > 0 ? 1 : 0);

        filler(buf, file->name.c_str(), &st, 0);
    }

    return 0;
}

static int Mkdir(const char *path, mode_t mode) {
    Context *ctx = (Context *)fuse_get_context()->private_data;
    const char *parentName = dirname(path);
    const char *dirName = basename(path);

    Dir *parent = FindDir(parentName, ctx);
    if (parent == nullptr) {
        return -ENOENT;
    }

    int ret = gp_camera_folder_make_dir(ctx->camera(), parentName,
            dirName, ctx->context());
    if (ret != GP_OK) {
        return gpresultToErrno(ret);
    }
    Dir *dir = new Dir(dirName);
    parent->addDir(dir);
    return 0;
}

static int Rmdir(const char *path) {
    Context *ctx = (Context *)fuse_get_context()->private_data;
    const char *parentName = dirname(path);
    const char *dirName = basename(path);

    Dir *parent = FindDir(parentName, ctx);
    Dir *dir = FindDir(path, ctx);
    if (parent == nullptr || dir == nullptr) {
        return -ENOENT;
    }

    int ret = gp_camera_folder_remove_dir(ctx->camera(), parentName, dirName,
            ctx->context());
    if (ret != GP_OK) {
        return gpresultToErrno(ret);
    }

    parent->removeDir(dir);
    delete dir;
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
    .truncate = Truncate,

    .getattr = Getattr,

    .readdir = Readdir,
    .mkdir = Mkdir,
    .rmdir = Rmdir,

    .create = Create,
    .open = Open,
    .release = Release,
    .unlink = Unlink,
    .read = Read,
    .write = Write,
    .flush = Flush,
};

int main(int argc, char **argv) {
    setlocale (LC_CTYPE,"en_US.UTF-8"); /* for ptp2 driver to convert to utf-8 */
    return fuse_main(argc, argv, &GPhotoFS2_Operations, NULL);
}
