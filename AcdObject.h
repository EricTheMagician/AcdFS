//
// Created by Eric Yen on 2016-09-30.
//

#ifndef ACDFS_ACDOBJECT_H
#define ACDFS_ACDOBJECT_H

#define ISO8601TIMESPEC "YYYY-MM-DDTHH:mm:ss.sssZ"


#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/document/value.hpp>
#include <sys/stat.h>
#include <map>
#include <vector>
class AcdObject;
typedef std::shared_ptr<AcdObject> AcdObjectPtr;
class AcdObject {
public:
    AcdObject(ino_t, bsoncxx::document::value);
    ~AcdObject();
private:
    friend class Filesystem;
    friend class AcdApi;
    friend class Account;
    friend class FileIO;
    bsoncxx::document::value m_document;
    bsoncxx::document::view m_view;

    std::string m_id, m_md5, m_parentid;
    uint64_t m_fileIV;
    std::string m_name, m_decodedname;
    bool isFile, isFolder;
    std::shared_ptr<std::vector<char>> m_folder_buffer;
    std::shared_ptr<std::map<std::string, AcdObjectPtr> > m_children;
    struct stat m_stat;

};

#endif //ACDFS_ACDOBJECT_H
