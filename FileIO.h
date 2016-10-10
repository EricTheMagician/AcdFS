//
// Created by Eric Yen on 2016-10-06.
//

#ifndef ACDFS_FILEIO_H
#define ACDFS_FILEIO_H

#include "AcdObject.h"
#include <iostream>
#include <fstream>
#include <atomic>
class AcdApi;
#define MAX_CACHE_SIZE (512*1024*1024) //2GB
#define BLOCK_DOWNLOAD_SIZE 4194304 //4MB
#define NUM_BLOCK_READ_AHEAD 12

//these two define the range over which we should start doing a readhead
#define BLOCKREADAHEADSTART 2097152
#define BLOCKREADAHEADFINISH 2359296


class __no_collision_download__;
typedef std::shared_ptr< __no_collision_download__> DownloadItem;

class FileIO {

public:
    FileIO(AcdObjectPtr,  int, AcdApi*);
    std::string read(const size_t &size, const off_t &off);
    void open();

private:
    std::string download(AcdApi *api, DownloadItem cache, std::string cacheName, uint64_t start, uint64_t end);
    std::string getFromCache( const size_t &size, const off_t &off);
    friend class Filesystem;
    friend class Account;
    friend class AcdApi;

    AcdObjectPtr m_file;
    bool m_is_uploading;
    bool m_is_cached;

    bool m_readable, m_writeable;
    int m_flags;
    std::atomic<size_t> last_write;

    std::fstream stream;
    AcdApi *api;
};


#endif //ACDFS_FILEIO_H
