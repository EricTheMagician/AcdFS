//
// Created by Eric Yen on 2016-09-30.
//

#include "AcdObject.h"
#include "easylogging++.h"
#include <bsoncxx/json.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <random>
#include <bsoncxx/builder/stream/document.hpp>
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

static std::random_device rd;
static std::mt19937 gen( rd() );
static std::uniform_int_distribution<> random_numbe_generator(0,1000);

time_t getTime(struct tm &t,int &year, int &month, int &day, int &hour, int &minutes, int &seconds){
    t.tm_year = year - 1900;
    t.tm_mon = month;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min = minutes;
    t.tm_sec = seconds;
    t.tm_isdst = 0;
//    t.tm_year = 16;
//    t.tm_mon = 8;
//    t.tm_mday = 8;
//    t.tm_hour = 8;
//    t.tm_min = 8;
//    t.tm_sec = 8;
//    t.tm_isdst = 0;
    return std::mktime(&t);
}

AcdObject::AcdObject(ino_t inode, bsoncxx::document::value document): isFolder(false), isFile(false){

    auto view = document.view();

    int mode = -1;
    if(view["mode"]){
        mode = view["mode"].get_int32();
    }

    memset(&m_stat, 0, sizeof(struct stat));
    if(view["kind"].get_utf8().value.to_string() == "FOLDER"){
        isFolder = true;
        m_stat.st_size = 4096;
        m_stat.st_nlink = 2;
        m_stat.st_mode = S_IFDIR | 0777;
    }else{
        isFile = true;
        const auto type = view["contentProperties"]["size"].type();
        if(type == bsoncxx::type::k_utf8){
            m_stat.st_size = atoll(view["contentProperties"]["size"].get_utf8().value.to_string().data());
        }else if(type == bsoncxx::type::k_int32){
            m_stat.st_size = view["contentProperties"]["size"].get_int32();
        }else if(type == bsoncxx::type::k_int64) {
            m_stat.st_size = view["contentProperties"]["size"].get_int64();
        }else{
            LOG(ERROR) << bsoncxx::to_json(view["contentProperties"]["size"]) <<             bsoncxx::to_string(type);
        }

        bsoncxx::document::element md5{view["contentProperties"]["md5"]};
        if(md5) {
            m_md5 = md5.get_utf8().value.to_string();
        }
        m_stat.st_nlink = 1;
        m_stat.st_mode = S_IFREG | 0744;

    }

    if(mode >=0){
        m_stat.st_mode = mode;
    }

    if(inode > 1) {
        //root has no name
        m_name = view["name"].get_utf8().value.to_string();
    }else{
        m_name = "root";
    }
    bsoncxx::document::element decodedName{view["decodedName"]};
//    if(decodedName){
//        m_decodedname = decodedName.get_utf8().value.to_string();
//    }

    m_id = view["id"].get_utf8().value.to_string();

    // set parent id if inode is > 1. an inode of 1 means root and root doesn't have a parent id
    if(inode > 1) {
        m_parentid = view["parents"][0].get_utf8().value.to_string();
    }

    //set fileIV
//    if(m_view["fileIV"]){
//        m_fileIV = m_view["fileIV"].get_int64();
//    }

    int year,month,day,hour,minute, seconds, milliseconds;
    struct tm t;
    memset(&t, 0, sizeof(struct tm));

    std::string date_string = view["createdDate"].get_utf8().value.to_string().data();
    sscanf(date_string.c_str(), "%d-%d-%dT%d:%d:%d.%dZ", &year, &month, &day, &hour, &minute, &seconds, &milliseconds);
    m_stat.st_ctim.tv_sec = getTime(t, year, month, day, hour, minute, seconds);
    m_stat.st_ctim.tv_nsec = milliseconds;


    date_string = view["modifiedDate"].get_utf8().value.to_string().data();
    sscanf(date_string.c_str(), "%d-%d-%dT%d:%d:%d.%dZ", &year, &month, &day, &hour, &minute, &seconds, &milliseconds);

    m_stat.st_mtim.tv_sec = getTime( t, year, month, day, hour, minute, seconds);
    m_stat.st_mtim.tv_nsec = milliseconds;

    m_stat.st_atim.tv_sec = m_stat.st_mtim.tv_sec;
    m_stat.st_atim.tv_nsec = m_stat.st_mtim.tv_nsec;

    m_stat.st_ino = inode;

    if(view["version"]){
        m_generation = view["version"].get_int32();
    }

};
AcdObject::~AcdObject(){
    m_folder_buffer.reset();
    m_children.reset();
}

AcdObject::AcdObject( ino_t inode, bsoncxx::document::value document, const char* name, mode_t mode ){

    memset(&m_stat, 0, sizeof(struct stat));
    auto view = document.view();
    m_name = view["name"].get_utf8().value.to_string();
    time_t timer;
    std::time(&timer);
    struct tm *tm = gmtime(&timer);

    std::stringstream id;
    int milliseconds = random_numbe_generator(gen);
    char bufid[26];
    bufid[26] = 0;
    std::snprintf(bufid, 26, "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
                  tm->tm_year + 1900,tm->tm_mon,tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, milliseconds);

    m_id = bufid;

    isFile = true;
    isFolder = !false;

    m_stat.st_size = 0;
    m_stat.st_nlink = 1;

    if(mode >=0 ){
        m_stat.st_mode = mode;
    }

    if(inode > 1) {
        m_name.assign(name);
    }else{
        m_name = "root";
    }


    struct tm t;
    memset(&t, 0, sizeof(struct tm));

    struct tm *ltime = localtime(&timer);

    m_stat.st_mtim.tv_sec = mktime(ltime);
    m_stat.st_mtim.tv_nsec = milliseconds;

    m_stat.st_atim.tv_sec = m_stat.st_mtim.tv_sec;
    m_stat.st_atim.tv_nsec = m_stat.st_mtim.tv_nsec;


    m_stat.st_ctim.tv_sec = m_stat.st_mtim.tv_sec;
    m_stat.st_ctim.tv_nsec = m_stat.st_mtim.tv_nsec;

    m_stat.st_ino = inode;
    m_parentid = view["parents"][0].get_utf8().value.to_string();

    m_generation = 1;

}

bsoncxx::document::value AcdObject::document(){
    struct tm *tm;
    time_t timer;
    int milliseconds;

    timer = m_stat.st_ctim.tv_sec;

    tm = localtime(&timer);
    milliseconds = m_stat.st_ctim.tv_nsec;
    char bufid[26];
    bufid[26] = 0;
    std::snprintf(bufid, 26, "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
                  tm->tm_year + 1900,tm->tm_mon,tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, milliseconds);


    timer = m_stat.st_mtim.tv_sec;
    tm = localtime(&timer);
    milliseconds = m_stat.st_mtim.tv_nsec;
    char bufid2[26];
    bufid2[26] = 0;
    std::snprintf(bufid2, 26, "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
                  tm->tm_year + 1900,tm->tm_mon,tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, milliseconds);

    bsoncxx::builder::stream::document doc;
    doc  //<< "$set" << open_document
         << "mode" << (int) m_stat.st_mode
         << "contentProperties" << open_document << "size" << m_stat.st_size << close_document
         << "id" << m_id << "createdDate" << bufid
         << "parents" << open_array << m_parentid << close_array
         << "name" << m_name
         << "modifiedDate" <<  bufid2
         << "status" << "AVAILABLE"
    ;

    if(isFile){
        doc << "kind" << "FILE";
    }else{
        doc<< "kind" << "FOLDER";
    }
    return doc.extract();
}

bool AcdObject::isUploaded(){

    int year,month,day,hour,minute, seconds, milliseconds;
    return sscanf(m_id.c_str(), "%d-%d-%dT%d:%d:%d.%dZ", &year, &month, &day, &hour, &minute, &seconds, &milliseconds) != 7;

}