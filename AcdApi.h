//
// Created by Eric Yen on 2016-09-27.
//

#ifndef ACDFS_ACDAPI_H
#define ACDFS_ACDAPI_H

#include "Account.h"
#include "AcdObject.h"

class AcdApi {
public:
    AcdApi(Account *account);
    void Sync();
    void SetEndPoints();
    std::string Download(AcdObjectPtr, uint64_t,uint64_t);
    virtual ~AcdApi();

private:
    friend class Account;
    int parseMessage(std::string);
    void GetNewAccessToken();
    void ConnectToDatabase();
//    std::string gunzipResponse(web::http::http_response &);
//    web::json::value convertStringToJson(std::string);
    Account *m_account;
    std::string m_sync_checkpoint;



};

// Error codes for errors
#define ACD_SUCCESS         0
#define ACD_TOKEN_EXPIRED   1

#endif //ACDFS_ACDAPI_H
