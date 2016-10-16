//
// Created by Eric Yen on 2016-09-30.
//

#ifndef ACDFS_ACDOBJECT_H
#define ACDFS_ACDOBJECT_H

#define ISO8601TIMESPEC "%FT%T.%z"


#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/document/value.hpp>
#include <sys/stat.h>
#include <map>
#include <vector>
#include <mutex>
class AcdObject;
typedef std::shared_ptr<AcdObject> AcdObjectPtr;
class AcdObject {
public:
    AcdObject(ino_t, bsoncxx::document::value);
    AcdObject(ino_t,bsoncxx::document::value document, const char* name, mode_t mode); //used to create a new file
    bsoncxx::document::value document();
    bool isUploaded();
    ~AcdObject();
private:
    friend class Filesystem;
    friend class AcdApi;
    friend class Account;
    friend class FileIO;

    std::string m_id, m_md5, m_parentid;
    std::string m_name;
    bool isFile, isFolder;
    std::shared_ptr<std::vector<char>> m_folder_buffer;
    std::shared_ptr<std::map<std::string, AcdObjectPtr> > m_children;
    struct stat m_stat;
    std::mutex m_lock;
    uint32_t  m_generation;

};

#endif //ACDFS_ACDOBJECT_H
