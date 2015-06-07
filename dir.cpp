#include "dir.h"
#include "file.h"

void Dir::addFile(File *file) {
    files[file->name] = file;
}

void Dir::removeFile(File *file) {
    files.erase(file->name);
}

void Dir::addDir(Dir *dir) {
    dirs[dir->name] = dir;
}

void Dir::removeDir(Dir *dir) {
    dirs.erase(dir->name);
}

File* Dir::getFile(const std::string& name) {
    auto it = files.find(name);
    if (it == files.end()) return nullptr;
    return it->second;
}

Dir* Dir::getDir(const std::string& name) {
    auto it = dirs.find(name);
    if (it == dirs.end()) return nullptr;
    return it->second;
}

