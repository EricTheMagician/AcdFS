//
// Created by Eric Yen on 2016-09-30.
//

#include "AcdObject.h"
#include "easylogging++.h"
#include <bsoncxx/json.hpp>
AcdObject::AcdObject(ino_t inode, bsoncxx::document::value document):m_document(document), isFolder(false), isFile(false){

    m_view = m_document.view();

    memset(&m_stat, 0, sizeof(struct stat));
    if(m_view["kind"].get_utf8().value.to_string() == "FOLDER"){
        isFolder = true;
        m_stat.st_size = 4096;
        m_stat.st_nlink = 2;
        m_stat.st_mode = S_IFDIR | 0777;
    }else{
        isFile = true;
        const auto type = m_view["contentProperties"]["size"].type();
        if(type == bsoncxx::type::k_utf8){
            m_stat.st_size = atoll(m_view["contentProperties"]["size"].get_utf8().value.to_string().data());
        }else if(type == bsoncxx::type::k_int32){
            m_stat.st_size = m_view["contentProperties"]["size"].get_int32();
        }else if(type == bsoncxx::type::k_int64) {
            m_stat.st_size = m_view["contentProperties"]["size"].get_int64();
        }else{
            LOG(ERROR) << bsoncxx::to_json(m_view["contentProperties"]["size"]) <<             bsoncxx::to_string(type);
        }

        bsoncxx::document::element md5{m_view["contentProperties"]["md5"]};
        if(md5) {
            m_md5 = md5.get_utf8().value.to_string();
        }
        m_stat.st_nlink = 1;
        m_stat.st_mode = S_IFREG | 0744;

    }

    if(inode > 1) {
        //root has no name
        m_name = m_view["name"].get_utf8().value.to_string();
    }else{
        m_name = "root";
    }
    bsoncxx::document::element decodedName{m_view["decodedName"]};
    if(decodedName){
        m_decodedname = decodedName.get_utf8().value.to_string();
    }

    m_id = m_view["id"].get_utf8().value.to_string();

    // set parent id if inode is > 1. an inode of 1 means root and root doesn't have a parent id
    if(inode > 1) {
        m_parentid = m_view["parents"][0].get_utf8().value.to_string();
    }

    //set fileIV
    if(m_view["fileIV"]){
        m_fileIV = m_view["fileIV"].get_int64();
    }


    struct tm t;
    strptime(m_view["createdDate"].get_utf8().value.to_string().data(), ISO8601TIMESPEC, &t);
    m_stat.st_ctime = mktime(&t);

    strptime(m_view["modifiedDate"].get_utf8().value.to_string().data(), ISO8601TIMESPEC, &t);
    m_stat.st_mtime = mktime(&t);
    m_stat.st_atime = mktime(&t);

    m_stat.st_ino = inode;

};
AcdObject::~AcdObject(){
    m_folder_buffer.reset();
    m_children.reset();
}

