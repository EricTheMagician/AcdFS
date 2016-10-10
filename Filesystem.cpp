//
// Created by Eric Yen on 2016-10-05.
//

#include "Filesystem.h"
#include <cstring>
#include <errno.h>
#include <memory>
#include <cstdlib>
#include <future>
#include "easylogging++.h"
#include "Account.h"
#include "FileIO.h"
#include  <inttypes.h>
//#define BOOST_THREAD_PROVIDES_FUTURE
//#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
//#include <booƒst/thread/future.hpp>

#include <boost/asio.hpp>
#include <boost/thread.hpp>


static const char *hello_str = "Hello World!\n";
static const char *hello_name = "hello";


inline Account* getAccount(fuse_req_t req){
    return static_cast<Account *>(fuse_req_userdata(req));

}

std::shared_ptr<AcdObject> Filesystem::getObjectFromInodeAndReq(fuse_req_t req, ino_t inode){

    Account *account = getAccount(req);
    auto inodeToObject = account->inodeToObject;
    auto cursor = inodeToObject.find(inode);

    if(cursor == inodeToObject.cend()){
        //object not found
        std::shared_ptr<AcdObject> empty;
        return empty;
    }

    return cursor->second;



}

void Filesystem::forget_multi(fuse_req_t req, size_t count, struct fuse_forget_data *forgets){
    fuse_reply_none(req);
//    fuse_reply_err(req,ENOSYS);
}

extern boost::asio::io_service io_service;
boost::asio::io_service io_service;
extern boost::thread_group threads;
boost::thread_group threads;

void Filesystem::init(void *userdata, struct fuse_conn_info *conn){
//boost::asio::io_service::work work(io_service);
    for (std::size_t i = 0; i < 32; ++i)
        threads.create_thread(boost::bind(&boost::asio::io_service::run, &io_service));
}

void Filesystem::access (fuse_req_t req, fuse_ino_t ino, int mask){
    fuse_reply_err(req, 0);
};

void Filesystem::statfs(fuse_req_t req, fuse_ino_t ino){
    struct statvfs stat{
            .f_bsize = 65536,
            .f_frsize=  65536,
            .f_blocks=  1000000,
            .f_bfree=  1000000,
            .f_bavail=  1000000,
            .f_files=  1000000,
            .f_ffree=  1000000,
            .f_favail=  1000000,
            .f_fsid=  1000000,
            .f_flag=  0,
    };
    fuse_reply_statfs(req, &stat);
}

void Filesystem::release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi){
    fuse_reply_err(req, 0);
}

void Filesystem::read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi) {
    FileIO *io = (FileIO *) fi->fh;
    auto fileSize = io->m_file->m_stat.st_size;
    size = off + size > fileSize ? fileSize - off : size;
    //std::async(std::launch::async, [io, &off, &size, &req]() {
        VLOG(7) << "(off,size) = (" << off << ", " << size  << ")";
        std::string buf = io->read(size, off);
        //    LOG(TRACE) << "filesize expected: " << size << ". Received: " << buf.length;
        fuse_reply_buf(req, buf.c_str(), buf.length());
    //});
}
void Filesystem::open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
    auto account = getAccount(req);
    auto cursor = account->inodeToObject.find(ino);
    if( cursor == account->inodeToObject.cend()){
        fuse_reply_err(req, ENOENT);
        return;
    }

    auto object = cursor->second;

    FileIO *io = new FileIO(object, fi->flags, account->api);

    fi->fh = (uint64_t) io;

    fuse_reply_open(req, fi);
}

void Filesystem::getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {

//    LOG(TRACE) << "getattr";

    auto object = getObjectFromInodeAndReq(req, ino);
    if(object == nullptr){
        fuse_reply_err(req, ENOENT);
        return;
    }
//        LOG(TRACE) << stbuf.st_ino;
//        LOG(TRACE) << stbuf.st_atimespec.tv_nsec;
//        LOG(TRACE) << stbuf.st_ctimespec.tv_nsec;
//        LOG(TRACE) << stbuf.st_mtimespec.tv_nsec;
//        LOG(TRACE) << stbuf.st_mode;

    fuse_reply_attr(req, &(object->m_stat), 180.0);



}

void Filesystem::lookup(fuse_req_t req, fuse_ino_t parent, const char *name) {
    std::async(std::launch::async, [&req, &parent, name](){
        std::string child_name(name);
        struct fuse_entry_param e;

        auto object = getObjectFromInodeAndReq(req, parent);
        if(object == nullptr){
            fuse_reply_err(req, ENOENT);
            return;
        }

        auto children = object->m_children;
        AcdObjectPtr childptr;
        if(children == nullptr){
            auto id = object->m_id;
            auto account = getAccount(req);
            childptr = account->doesParentHaveChild(id, name);

            if(childptr == nullptr){
                fuse_reply_err(req, ENOENT);
                return;
            }

        }else {
            auto cursor = children.get()->find(child_name);
            if(cursor == children->cend()){
                fuse_reply_err(req, ENOENT);
                return;
            }
            childptr = cursor->second;

        }

        if(childptr == nullptr){
            fuse_reply_err(req, ENOENT);
            return;
        }


        memset(&e, 0, sizeof(e));
        e.attr = childptr->m_stat;
        e.ino = e.attr.st_ino;
        e.attr_timeout = 180.0;
        e.entry_timeout = 180.0;

        fuse_reply_entry(req, &e);
    } );
}

#define min(x, y) ((x) < (y) ? (x) : (y))

static int reply_buf_limited(fuse_req_t req, const char *buf, size_t bufsize,
                             off_t off, size_t maxsize)
{
    if (off < bufsize)
        return fuse_reply_buf(req, buf + off,
                              min(bufsize - off, maxsize));
    else
        return fuse_reply_buf(req, NULL, 0);
}

void Filesystem::readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi) {

    std::async(std::launch::async, [&]() {


        auto folder = getObjectFromInodeAndReq(req, ino);

        if (folder == nullptr) {
            fuse_reply_err(req, ENOENT);
        }

        if (folder->isFile) {
            fuse_reply_err(req, ENOTDIR);
            return;
        }

        // load the buffer from the previous call, or create a new buffer
        if (folder->m_folder_buffer == nullptr) {
            Account *account = getAccount(req);
            auto children = account->getChildrenFromObject(folder);
            const int totalSize = 256 * children.size();
            auto buffer = std::make_shared<std::vector<char>>(totalSize);
            size_t accumulated_size = 0;
            size_t sz;
            for (auto child : children) {

                sz = fuse_add_direntry(req, NULL, 0, child->m_name.data(), NULL, 0);
                fuse_add_direntry(req, buffer->data() + accumulated_size, totalSize - accumulated_size,
                                  child->m_name.data(), &(child->m_stat),
                                  accumulated_size + sz);
                accumulated_size += sz;

            }
            buffer->resize(accumulated_size);

            folder->m_folder_buffer = buffer;
        }
        auto buffer = folder->m_folder_buffer.get();
        reply_buf_limited(req, buffer->data(), buffer->size(), off, size);
    });

}