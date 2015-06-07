#ifndef __GPHOTOFS2_DIR_H_
#define __GPHOTOFS2_DIR_H_

#include <string>
#include <map>

class File;

struct Dir {
    std::string name;

    bool listed;
    std::map<std::string, File*> files;
    std::map<std::string, Dir*> dirs;

    Dir(const std::string& name) : name(name), listed(false) {}

    void addFile(File *file);
    void removeFile(File *file);
    File* getFile(const std::string& name);

    void addDir(Dir *dir);
    void removeDir(Dir *dir);
    Dir* getDir(const std::string& name);
};

#endif // __GPHOTOFS2_DIR_H_
