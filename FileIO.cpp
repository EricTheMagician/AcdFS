//
// Created by Eric Yen on 2016-10-06.
//

#define DOWNLOADINGNUMBERLOG 6

#include "FileIO.h"
#include "AcdApi.h"
#include "easylogging++.h"
#include <boost/filesystem.hpp>
#include <ctime>
#include <queue>
#include <map>
#include <thread>
#include <sstream>
#include <future>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#define CACHEPATH "/tmp/mnt"

//#define O_CREATE 32768
//#define O_RDONLY  32768
//#define O_WRONLY 32769
//#define O_RDWR  32770
//#define O_APPEND  33792



#define MIN(x,y) x<y?x:y
extern boost::asio::io_service io_service;
extern   boost::thread_group threads;
namespace fs = boost::filesystem;

static fs::path cache_path(CACHEPATH);

class __no_collision_download__{
public:
    __no_collision_download__(){}
    ~__no_collision_download__(){}
    std::string buffer;
    std::string cacheName;
    std::shared_future<std::string> future;
//    bool operator<(const  __no_collision_download__& rhs) const
//    {
//        return  last_access < rhs.last_access;
//    }
};

typedef std::shared_ptr< __no_collision_download__> DownloadItem;
typedef std::weak_ptr< __no_collision_download__> WeakBuffer;

/*
class CacheCompare{
public:
bool operator()(const DownloadItem b, const DownloadItem a){
    if( a && b){
        if (a->buffer.length() > 0 && b->buffer.length() > 0) {
            if(a.use_count() == b.use_count()) {
                return a->last_access < b->last_access;
            }else{
                return a.use_count()  > b.use_count();
            }
        }

        if (a->buffer.length() > 0) return true;
        if (b->buffer.length() > 0) return false;
        return a->last_access < b->last_access;
    }
    if(a.use_count() > 0){
        return true;
    }

    if(b.use_count()> 0){
        return false;
    }

    return false;

}
};
*/
//static std::priority_queue<DownloadItem, std::vector<DownloadItem>, CacheCompare > PriorityCache{};
static std::queue<DownloadItem> PriorityCache{};
static std::map<std::string, WeakBuffer> DownloadCache;

inline uint64_t getChunkStart(uint64_t start){
    uint64_t chunkNumber = start / BLOCK_DOWNLOAD_SIZE;
    return chunkNumber * BLOCK_DOWNLOAD_SIZE;
}


FileIO::FileIO(AcdObjectPtr file, int flag, AcdApi *api): api(api), m_file(file), m_flags(flag), last_write(0){
    m_writeable = flag & (O_WRONLY | O_RDWR | O_APPEND);
    m_readable = flag & (O_RDONLY | O_RDWR);
}

void FileIO::open(){

    struct tm ltm;
    char *end = strptime(m_file->m_id.c_str(), "%Y/%b/%d ", &ltm);
    if(end == NULL){
        //file is not cached on the hdd
        m_is_cached = false;
        return;
    }

    m_is_cached = true;
    fs::path file_location = cache_path;
    file_location /= m_file->m_id;
    stream.open(file_location.string());

    return;
}

std::string FileIO::read(const size_t &size, const off_t &off) {
//    const char* buffer;
//    if(m_is_cached){
//        buffer = (char *) malloc(size);
//        stream.seekg(off);
//        stream.read((char *)buffer, size);
//        return buffer;
//    }
    return getFromCache(size, off );
}

static const int max_yield_count = 10000;

std::string FileIO::getFromCache( const size_t &size, const off_t &off ){

    uint64_t chunkStart = getChunkStart(off);
    auto fileSize = m_file -> m_stat.st_size;
//    size = size + off > fileSize ? fileSize-off-1: size;
    std::stringstream ss;
    ss << m_file->m_id;
    ss<< "\t";
    ss << chunkStart;
    std::string cacheName = ss.str();
    std::vector<uint64_t> chunksToDownload;

    auto cursor = DownloadCache.find(cacheName);
    std::string buffer;
    bool spillOver =  (off + size-1) >= (chunkStart+ BLOCK_DOWNLOAD_SIZE);

    if (cursor != DownloadCache.cend()) {

        WeakBuffer cache = cursor->second;
        DownloadItem item = cache.lock();
        if(item) {
            if(item->buffer.length() == 0){
                int yield_count = 0;

                std::shared_future<std::string> future(item->future);

                while( (!future.valid()) && (item->buffer.length() == 0) && (yield_count < max_yield_count) ){
//                while( !(item->future.valid()) && (item->buffer.length() == 0) ){
                    std::this_thread::yield();
                    yield_count++;
                }
                if(yield_count == max_yield_count){
                        // max yield reached;
                    return getFromCache(size,off);
                }


                if( (item->buffer.length() == 0) ){
                    VLOG(7) << "Waiting for 10 seconds!";
                    std::future_status status = item->future.wait_for(std::chrono::seconds(10));
                    if(status == std::future_status::timeout){
                        return getFromCache(size, off);
                    }
                }

            }

            uint64_t start = off % BLOCK_DOWNLOAD_SIZE;
            uint64_t size2 = spillOver? BLOCK_DOWNLOAD_SIZE-off : size;
//            LOG(TRACE) << "(off,size) = (" << start << ", " << size2  << ")";
//            item->last_access = time(NULL);
            buffer += item->buffer.substr(start, size2);
        }else{
            DownloadCache.erase(cacheName);
            chunksToDownload.push_back(chunkStart);
        }


    }else{
        chunksToDownload.push_back(chunkStart);

    }

    if(spillOver) {

        uint64_t chunkStart2 = chunkStart + BLOCK_DOWNLOAD_SIZE;
        ss.str(std::string());
        ss << "\t";
        ss <<m_file->m_id;
        ss << chunkStart2;
        cacheName = ss.str();
        cursor = DownloadCache.find(cacheName);

        if (cursor != DownloadCache.cend()) {
            WeakBuffer cache = cursor->second;
            DownloadItem item = cache.lock();
            if(item) {
                if(item->buffer.length() == 0){
                    std::shared_future<std::string> future(item->future);
                    int yield_count = 0;
                    while( (!future.valid()) && (item->buffer.length() == 0) && (yield_count < max_yield_count) ){
//                    while( !(item->future.valid()) && (item->buffer.length() == 0) ){
                        std::this_thread::yield();
                        yield_count++;
                    }

                    if(yield_count == max_yield_count){
                        // max yield reached;
                        return getFromCache(size,off);
                    }

                    if( (item->buffer.length() == 0) ){
                        std::future_status status = item->future.wait_for(std::chrono::seconds(10));
                        if(status == std::future_status::timeout){
                            return getFromCache(size, off);
                        }
                    }

                    item.reset();

//                    LOG(TRACE) << "Buffer length was either valid :\t" <<item->buffer.length() ;
                }

                uint64_t start = 0;
                uint64_t size2 = (off+size)%BLOCK_DOWNLOAD_SIZE;

                buffer += item->buffer.substr(start, size2);
//                item->last_access = time(NULL);

            }else{
                DownloadCache.erase(cacheName);
                chunksToDownload.push_back(chunkStart2);
            }

        } else {
            chunksToDownload.push_back(chunkStart2);
        }

    }

    bool done = chunksToDownload.empty();
    int off2 = off % BLOCK_DOWNLOAD_SIZE;
    if( off2 >= BLOCKREADAHEADSTART && off2 <= BLOCKREADAHEADFINISH ){
        uint64_t start = chunkStart;
        start += spillOver ? 2*BLOCK_DOWNLOAD_SIZE : BLOCK_DOWNLOAD_SIZE;
        uint64_t temp = 0;
        for(uint64_t i = 0; i < NUM_BLOCK_READ_AHEAD; i++){
            temp = start + i*BLOCK_DOWNLOAD_SIZE;
            if (temp > fileSize){
                break;
            }
            chunksToDownload.push_back(temp);
        }
    }

    for(uint64_t &start: chunksToDownload){

        ss.str(std::string());
        ss << m_file->m_id;
        ss << "\t";
        ss << start;
        cacheName = ss.str();
        cursor = DownloadCache.find(cacheName);
        if (cursor == DownloadCache.cend()) {
            DownloadItem cache = std::make_shared<__no_collision_download__>();
//            cache->last_access = time(NULL);
            cache->cacheName = cacheName;

            DownloadCache.insert(std::make_pair(cacheName,cache));
            PriorityCache.push(cache);
            cache->future =std::async(std::launch::async, &FileIO::download, this, api,cache, cacheName, start, start + BLOCK_DOWNLOAD_SIZE - 1);

         }
    }

    if(done){
        return buffer;
    }else{
        return getFromCache( size, off);
    }

}

std::string FileIO::download(AcdApi *api, DownloadItem cache, std::string cacheName, uint64_t start, uint64_t end){
//    VLOG(DOWNLOADINGNUMBERLOG) << "Downloading " << cacheName;
    LOG(TRACE) << "Downloading " << cacheName;
    cache->buffer = api->Download(m_file,start,end);
//    auto f = std::async(std::launch::async, &AcdApi::Download, api, m_file,start,end);
//    cache->buffer = f.get();
    while( (PriorityCache.size()*BLOCK_DOWNLOAD_SIZE) > MAX_CACHE_SIZE){
        auto top = PriorityCache.front();
        PriorityCache.pop();
//        std::cout << "Delete use count " << top.use_count() <<std::endl;
        DownloadCache.erase(top->cacheName);
        top.reset();
    }
    return cache->buffer;

}