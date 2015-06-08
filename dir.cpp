#include "dir.h"
#include "file.h"

#include <mutex>

using namespace std;

void Dir::addFile(File *file) {
    lock_guard<mutex> guard(mutex);
    files[file->name] = file;
}

void Dir::removeFile(File *file) {
    lock_guard<mutex> guard(mutex);
    files.erase(file->name);
}

void Dir::addDir(Dir *dir) {
    lock_guard<mutex> guard(mutex);
    dirs[dir->name] = dir;
}

void Dir::removeDir(Dir *dir) {
    lock_guard<mutex> guard(mutex);
    dirs.erase(dir->name);
}

File* Dir::getFile(const std::string& name) {
    lock_guard<mutex> guard(mutex);
    auto it = files.find(name);
    if (it == files.end()) return nullptr;
    return it->second;
}

Dir* Dir::getDir(const std::string& name) {
    lock_guard<mutex> guard(mutex);
    auto it = dirs.find(name);
    if (it == dirs.end()) return nullptr;
    return it->second;
}

bool Dir::empty() {
    lock_guard<mutex> guard(mutex);
    return files.empty() && dirs.empty();
}

Dir::~Dir() {
    for (auto it : files) {
        delete it.second;
    }
    for (auto it : dirs) {
        delete it.second;
    }
}
