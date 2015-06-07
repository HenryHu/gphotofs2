#include "context.h"
#include "dir.h"
using namespace std;

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
    statCache_ = nullptr;
}

Context::~Context() {
    if (abilities_) gp_abilities_list_free(abilities_);
    if (camera_) gp_camera_unref(camera_);
    if (context_) gp_context_unref(context_);
    if (statCache_) delete statCache_;
}
