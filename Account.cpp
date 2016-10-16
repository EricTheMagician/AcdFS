//
// Created by Eric Yen on 2016-09-27.
//

#include "Account.h"
#include "AcdApi.h"
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/pool.hpp>
#include "easylogging++.h"

using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;
using bsoncxx::builder::basic::kvp;
using std::endl;
using std::cout;


extern mongocxx::pool pool(  std::move(mongocxx::uri("mongodb://localhost?minPoolSize=4&maxPoolSize=16") ) );



Account::Account(std::string clientAccessToken, std::string clientRefreshToken):inode_count(1){
    LOG(TRACE) << "Account initialized";

    m_clientAccessToken = clientAccessToken;
    m_clientRefreshToken = clientRefreshToken;

}

Account::~Account(){
    inodeToObject.clear();
    idToObject.clear();
}

AcdObjectPtr Account::doesParentHaveChild(std::string id, const char *childName) {

    mongocxx::pool::entry conn = pool.acquire();
    mongocxx::database client =     conn->database(DATABASENAME);
    mongocxx::collection data =     client[DATABASEDATA];

    auto value = data.find_one(document{} << "parents.0" << id << "name" << childName << finalize);
    AcdObjectPtr object;
    if(value){
        std::string id = bsoncxx::document::view(*value)["id"].get_utf8().value.to_string();
//        LOG(TRACE) << "Parent with id " << id << " had child with name " << childName << " and id " << id;
        object = idToObject.at(id);
    }else{
//        LOG(TRACE) << "Parent with id " << id << " did not have child with name " << childName;
    }
    return object;
}
void Account::fillCache(){

    LOG(TRACE) << "Filling Cache";
    mongocxx::pool::entry conn = pool.acquire();
    mongocxx::database client =     conn->database(DATABASENAME);
    mongocxx::collection db = client[DATABASEDATA];
    auto cursor = db.find(document{}<<finalize);

//    auto cursor = getDB()->data.find(document{}<<finalize);
    for(auto doc: cursor){
        bsoncxx::document::element ele{doc["isRoot"]};
        std::shared_ptr<AcdObject> object;
        bsoncxx::document::value value{doc};
        ino_t inode;

        //get the inode of the object
        if(ele){
            //if it's root
            inode = 1;
        }else{
            inode = inode_count.fetch_add(1) + 1;
        }

        object = std::make_shared<AcdObject>(inode, value);
        std::string id = doc["id"].get_utf8().value.to_string();

        idToObject[id] = object;
        inodeToObject[inode] = object;
    }
    LOG(TRACE) << "idToObject has " << idToObject.size() << " items.";
    LOG(TRACE) << "inodeToObject has " << inodeToObject.size() << " items.";
}

const std::string &Account::getClientRefreshToken() const {
    return m_clientRefreshToken;
}

std::vector<std::shared_ptr<AcdObject>> Account::getChildrenFromObject(AcdObjectPtr parent) {

    std::vector<std::shared_ptr<AcdObject>> children;
    parent->m_lock.lock();
    if( (parent->m_children==nullptr) || parent->m_children->size() == 0) {
        mongocxx::options::find opts{};
        opts.projection(document{} << "id" << 1 << finalize);
//    auto documents = getDB()->data.find(document{} << "parents.0" << id << finalize, opts);
        mongocxx::pool::entry conn = pool.acquire();
        mongocxx::database client = conn->database(DATABASENAME);
        mongocxx::collection data = client[DATABASEDATA];
        auto documents = data.find(document{} << "parents.0" << parent->m_id << finalize, opts);
        int count = 0;

        auto shared_map = std::make_shared<std::map<std::string, AcdObjectPtr>>();
        std::map<std::string, AcdObjectPtr> *map = shared_map.get();

        for (auto doc: documents) {
            std::string child_id = doc["id"].get_utf8().value.to_string();
            auto child = idToObject.at(child_id);
//        LOG(TRACE) << "found child with name " << child->m_name << " and child id " << child_id;
            children.push_back(child);
            count++;
            (*map)[child->m_name] = child;
        }

        parent->m_children = shared_map;

        LOG(TRACE) << "parent with id " << parent->m_id << " has " << count << " children";
    }else{
        for(auto &cursor: *(parent->m_children.get()) ){
//            LOG(TRACE) << cursor.second->m_name;
            children.push_back(cursor.second);
        }
    }
    parent->m_lock.unlock();


    return children;

}

const std::string &Account::get_metadataUrl() const {
    return m_metadataUrl;
}

const std::string &Account::get_contentUrl() const {
    return m_contentUrl;
}

void Account::setNewAccessToken(const std::string accessToken, int expiresIn ){
    VLOG(9) << "Setting new access tokens: " << accessToken;
    m_clientAccessToken = accessToken;

}

void Account::setClientAccessToken(const std::string &clientAccessToken) {
    Account::m_clientAccessToken = clientAccessToken;
}

AcdObjectPtr Account::createNewChild(AcdObjectPtr parent, const char *name, mode_t mode){

    document child;
    child
          << "name" << name
          << "parents" << open_array << parent->m_id << close_array
          << "contentProperties" << open_document << "size" << 0 << close_document;
    auto inode = inode_count.fetch_add(1) + 1;

    //create a new object
//    AcdObjectPtr object = std::make_shared<AcdObject>(inode, bsoncxx::document::value{child}, name, mode);
    AcdObjectPtr object = std::make_shared<AcdObject>(inode, child.extract(), name, mode);

    //save the new object in memory
    inodeToObject[inode] = object;
    idToObject[object->m_id] = object;

    //reset the parent folder buffer
    parent->m_folder_buffer.reset();

    //add to parent children list
    auto shared_map = parent->m_children;
    std::map<std::string, AcdObjectPtr> *map = shared_map.get();
    (*map)[std::string(name)] = object;

    mongocxx::pool::entry conn = pool.acquire();
    mongocxx::database client = conn->database(DATABASENAME);
    mongocxx::collection data = client[DATABASEDATA];
    data.insert_one(
            object->document()
    );

    return object;
}

void Account::updateFileAfterUploading(AcdObjectPtr file, std::string response) {

    auto doc = bsoncxx::from_json(response.c_str());
    auto view = doc.view();
    auto id = view["id"].get_utf8().value.to_string();
    auto old_id = file->m_id;
    file->m_id = id;
    idToObject[id] = file;
    idToObject.erase(old_id);

    mongocxx::pool::entry conn = pool.acquire();
    mongocxx::database client = conn->database(DATABASENAME);
    mongocxx::collection data = client[DATABASEDATA];
    using bsoncxx::builder::concatenate;

    data.update_one(
            document{} << "id" << old_id << finalize,
            document{} << "$set" << open_document << concatenate(doc.view()) << close_document << finalize
    );


}