//
// Created by Eric Yen on 2016-10-05.
//

#ifndef ACDFS_FILESYSTEM_H
#define ACDFS_FILESYSTEM_H
#include <fuse3/fuse_lowlevel.h>
#include "AcdObject.h"

class Filesystem {
public:
    static void init(void *userdata, struct fuse_conn_info *conn);
    static void statfs (fuse_req_t req, fuse_ino_t ino);
    static void write(fuse_req_t req, fuse_ino_t ino, const char *buf,
                      size_t size, off_t off, struct fuse_file_info *fi);
    static void create(fuse_req_t req, fuse_ino_t parent, const char *name,
            mode_t mode, struct fuse_file_info *fi);
    static void lookup (fuse_req_t req, fuse_ino_t parent, const char *name);
    static void getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
    static void readdir(fuse_req_t req, fuse_ino_t ino, size_t size,
                        off_t off, struct fuse_file_info *fi);
    static void open(fuse_req_t req, fuse_ino_t ino,
                     struct fuse_file_info *fi);
    static void read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
                     struct fuse_file_info *fi);
    static void access (fuse_req_t req, fuse_ino_t ino, int mask);
    static void release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
    static void forget_multi(fuse_req_t req, size_t count, struct fuse_forget_data *forgets);
private:
    static std::shared_ptr<AcdObject> getObjectFromInodeAndReq(fuse_req_t , ino_t inode);
};


#endif //ACDFS_FILESYSTEM_H
