#ifndef __GPHOTOFS2_DIR_H_
#define __GPHOTOFS2_DIR_H_

#include <string>
#include <map>

#include <mutex>

class File;

struct Dir {
    std::string name;

    bool listed;
    std::map<std::string, File*> files;
    std::map<std::string, Dir*> dirs;
    std::mutex lock;

    Dir(const std::string& name) : name(name), listed(false) {}
    ~Dir();

    void addFile(File *file);
    void removeFile(File *file);
    File* getFile(const std::string& name);

    void addDir(Dir *dir);
    void removeDir(Dir *dir);
    Dir* getDir(const std::string& name);

    bool empty();
};

#endif // __GPHOTOFS2_DIR_H_
