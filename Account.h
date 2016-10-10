//
// Created by Eric Yen on 2016-09-27.
//

#ifndef ACDFS_ACCOUNT_H
#define ACDFS_ACCOUNT_H


#include "AcdObject.h"
#include <atomic>
#include <ctime>
class AcdApi;

typedef std::shared_ptr<AcdObject> AcdObjectPtr;
class Account {

public:
    Account(std::string, std::string);
    ~Account();
    void setClientAccessToken(const std::string &clientAccessToken);

    const std::string &getClientRefreshToken() const;

    void setNewAccessToken(const std::string accessToken, int expiresIn );

    void fillCache();
    std::vector<AcdObjectPtr> getChildrenFromObject(AcdObjectPtr);
    AcdObjectPtr doesParentHaveChild(std::string id, const char *childName);

    const std::string &get_metadataUrl() const;

    const std::string &get_contentUrl() const;

private:
    void updateMapping(ino_t, std::string, AcdObject &);
private:
    friend class AcdApi;
    friend class Filesystem;



    std::string m_clientAccessToken;
    std::string m_clientRefreshToken;
    std::string m_metadataUrl;
    std::string m_contentUrl;
    std::atomic<ino_t> inode_count;
    std::map<ino_t, std::shared_ptr<AcdObject>> inodeToObject;
    std::map<std::string, std::shared_ptr<AcdObject>> idToObject;
    std::time_t expiresIn;
    AcdApi *api;


//    bsoncxx::document::view view{};
};

#define DATABASEDATA "data"
#define DATABASESETTINGS "settings"
#define DATABASENAME "AcdFS"


#endif //ACDFS_ACCOUNT_H
