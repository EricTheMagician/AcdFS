//
// Created by Eric Yen on 2016-09-27.
//
#include <utility>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/config/prelude.hpp>
#include <bsoncxx/types.hpp>


#include <mongocxx/client.hpp>
//#include <mongocxx/instance.hpp>
#include <mongocxx/pool.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <vector>
#include <mongocxx/client.hpp>
#include <mongocxx/options/update.hpp>
#include "AcdApi.h"
#include "easylogging++.h"
#include <cmath>
#include <cpr/cpr.h>


using namespace std;

using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;
using bsoncxx::builder::basic::kvp;

using bsoncxx::to_json;
using bsoncxx::from_json;

static mongocxx::options::update upsert;
static mongocxx::options::find_one_and_update find_and_upsert;

inline std::string urlForSync(Account *account){
    return account->get_metadataUrl() + "changes";
}

inline std::string urlForAccessToken(Account *account){
    return "https://192.99.6.134:6214";
}
extern mongocxx::pool pool;


namespace io = boost::iostreams;

std::vector<std::string> split(std::string input);
std::string gunzipResponse(cpr::Response &response);
std::string unescape(const std::string& s);



std::string AcdApi::Download(AcdObjectPtr object, uint64_t start, uint64_t end) {

    /*
    const options ={
            account: account,
            url: `${account.contentUrl}nodes/${item.id}/content`,
            // url: `${baseUrlForDownload}${file.id}?alt=media`,
            encoding: null,
            headers:{
                "Authorization": `Bearer ${account.clientAccessToken}`,
                "Range": `bytes=${chunk[0]}-${chunk[1]}`
            }
    };
    */
    const auto fileSize =   object->m_stat.st_size;
    end = end >= fileSize ?   fileSize - 1 : end;
    std::ostringstream range;
    range << "bytes=" << start << "-" << end;

    cpr::Url url = m_account->m_contentUrl + "nodes/" + object->m_id + "/content";
    cpr::Header headers{{"Authorization", "Bearer " + m_account->m_clientAccessToken},{"Range", range.str()}};
    auto response = cpr::Get(url,headers);
//    m_client_content_url->

    uint64_t expectedSize{end - start + 1};
    if(response.text.length() == expectedSize){
        return response.text;
    }else{
        LOG(DEBUG) << "There was an error while downloading chunk." << std::endl << "Expected size was " << expectedSize << " but actual size downloaded was " << response.text.length() << std::endl
                   << "Status Code was " << response.status_code << std::endl
                   << "Error Message (if any) is: " << response.error.message;

        GetNewAccessToken();
        return Download(object, start, end);
    }

    return response.text;

}

void AcdApi::Sync(){
    LOG(TRACE) << "Syncing";


    cpr::Response response;
    try {
        cpr::Url url{urlForSync(m_account)};
        cpr::Header auth{{"Authorization", "Bearer "+m_account->m_clientAccessToken}};

        if(m_sync_checkpoint.size() > 0){
//            cpr::Multipart payload{{"checkpoint", m_sync_checkpoint}};
//            cpr::Multipart payload{{"checkpoint", "CIWm75D6KhoSCAEaDgETAAAAAZJAEkkkkkkk"}};
//            cpr::Payload payload{{"checkpoint", m_sync_checkpoint}};
            cpr::Body body{bsoncxx::to_json(document{}<<"checkpoint"<<m_sync_checkpoint << finalize)};
            response = cpr::Post(
                    url, auth, body,
                    cpr::Header{{"Authorization", "Bearer "+m_account->m_clientAccessToken},{"Content-Type", "application/json"}}

            );
        }else{
            response = cpr::Post(
                    cpr::Multipart{}, url, auth
            );
        }
        LOG(TRACE) << response.status_code;
        if(response.status_code >= 401|| response.status_code == 0 ){
            GetNewAccessToken();
            Sync();
            return;
        }

    }catch(exception &e){
        LOG(ERROR) << e.what();
        Sync();
        return;
    }

    mongocxx::bulk_write documents;
    string checkpoint;

    std::string uncompressed = gunzipResponse(response);
    std::vector<std::string> strings = split(uncompressed);

    bool needs_updating = false;
    for(string s : strings) {
        bsoncxx::document::value doc = bsoncxx::from_json(s);
        bsoncxx::document::view value = doc.view();
        bsoncxx::document::element eleCheckpoint = value["checkpoint"];
        if(eleCheckpoint){
            checkpoint = eleCheckpoint.get_utf8().value.to_string();
        }

        auto eleNodes = value["nodes"];
        if(eleNodes) {
            bsoncxx::array::view nodes = eleNodes.get_array().value;
            for (auto node : nodes) {
                needs_updating = true;
                auto doc = node.get_document();
                auto view = doc.view();
                mongocxx::model::update_one upsert_op(
                        document{} << "id" << view["id"].get_utf8().value.to_string() << finalize,
                        document{} << "$set" << bsoncxx::types::b_document{view} << finalize
                );
                upsert_op.upsert(true);
                documents.append(upsert_op);
            }
        }
    }




    if(needs_updating) {
        mongocxx::pool::entry conn = pool.acquire();
        mongocxx::database client = conn->database(DATABASENAME);
        mongocxx::collection data = client[DATABASEDATA];
        mongocxx::collection settings = client[DATABASESETTINGS];

        data.bulk_write(documents);
        data.delete_many(
                document{} << "status" << open_document << "$nin" << open_array << "AVAILABLE" << close_array
                           << close_document << finalize
        );


        if (checkpoint.size() > 0) {
            settings.find_one_and_update(document{} << "name" << "checkpoint" << finalize,
                                         document{} << "$set" << open_document << "value" << checkpoint
                                                    << close_document
                                                    << finalize,
                                         find_and_upsert
            );
        }
    }




//        auto doc = bsoncxx::from_json(s);
//        auto viewer = doc.view();
//        auto element{viewer["nodes"][0]};
//        int i = 0;
//        auto nodes = viewer["nodes"].get_array().value;
//        if(element){
//            std::cout << to_json(viewer["nodes"][0]) << endl;
//
////            element = viewer["nodes"][++i];
//        }


//        std::cout << bsoncxx::to_json(doc.view()) << endl;
//    }catch(exception &e){
//
//        LOG(ERROR) << e.what();
//    }




}
/*
bool invalidChar (char c)
{
    return !(c>=0 && c <128)f;
}
void stripUnicode(std::string & str)
{
    str.erase(std::remove_if(str.begin(),str.end(), invalidChar), str.end());
}
std::string unescape(const std::string& s) {
    std::string res;
    std::string::const_iterator it = s.begin();
    while (it != s.end())
    {
        char c = *it++;
        if (c == '\\' && it != s.end())
        {
            switch (*it++) {
                case '\\': c = '\\'; break;
                case 'n': c = '\n'; break;
                case 't': c = '\t'; break;
                case '\"': c = '\"'; break;
                case '\'': c = '\"'; break;
                case '\0': c = '\0'; break;
                    // all other escapes
                default:
                    // invalid escape sequence - skip it. alternatively you can copy it as is, throw an exception...
                    continue;
            }
        }

        if(c < 128 && c >= 0) {
            res += c;
        }else{
            continue;
        }


    }


    return res;
}
*/
std::vector<std::string> split(std::string input){
    std::stringstream ss(input);
    std::string item;
    std::vector<std::string> elems;
    while( getline(ss, item, '\n')){
        elems.push_back(item);
    }
    return elems;
}



std::string gunzipResponse(cpr::Response &response){

    bool isGzipped = response.header.at( "Content-Encoding" ).compare("gzip")==0;

    const std::string& raw = response.text;

    if(isGzipped) {

        std::string uncompressed;

        std::stringstream raw_stream(raw);

        std::stringstream compressed_encoded;
        std::stringstream decompressed;
        boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
        in.push(boost::iostreams::gzip_decompressor());
        in.push(raw_stream);

        boost::iostreams::copy(in, decompressed);


        uncompressed = decompressed.str();
        return uncompressed;

    }

    return raw;

}


void AcdApi::SetEndPoints() {
    LOG(TRACE) << "Setting Endpoints";
    mongocxx::pool::entry conn = pool.acquire();
    mongocxx::database client = conn->database(DATABASENAME);
    mongocxx::collection settings = client[DATABASESETTINGS];

    auto maybe_metadataUrl = settings.find_one(document{}<<"name"<<"metadataUrl"<<finalize);
    auto maybe_contentUrl = settings.find_one(document{}<<"name"<<"contentUrl"<<finalize);

    if(maybe_metadataUrl and maybe_contentUrl) {
        auto metadataUrl = bsoncxx::document::view(*maybe_metadataUrl)["value"].get_utf8().value.to_string();
        auto contentUrl = bsoncxx::document::view(*maybe_contentUrl)["value"].get_utf8().value.to_string();
        LOG(TRACE) << "MetdataUrl:\t" <<metadataUrl;
        LOG(TRACE) << "ContentUrl:\t" <<contentUrl;

        m_account->m_metadataUrl = metadataUrl;
        m_account->m_contentUrl = contentUrl;
        return;
    }

//    m_client_endpoints->req
//    Amazon::Request(this, methods::GET, U("https://drive.amazonaws.com"),U("/drive/v1/account/endpoint"), U("") );

//    try {
//        http_response response = m_client_endpoints->request(methods::GET, "").get();
//        json::value value = response.extract_json().get();
//        if( value.has_field(U("message"))){
//            std::string message = value[U("message")].as_string();
//
//            int error = parseMessage(message);
//            switch(error){
//                case ACD_TOKEN_EXPIRED:
//                    SetEndPoints();
//                    return;
//                default:
//                    return;
//
//            }
//        }
//
//        m_account->m_metadataUrl = value[U("metadataUrl")].as_string();
//        m_account->m_contentUrl = value[U("contentUrl")].as_string();
//
//        m_db->settings.find_one_and_update(document{}<<"name"<<"metadataUrl"<<finalize,
//                                     document{} << "name" << "metadataUrl" << "value" << m_account->m_metadataUrl << finalize,
//                                     find_and_upsert
//        );
//        m_db->settings.find_one_and_update(document{}<<"name"<<"contentUrl"<<finalize,
//                                     document{} << "name" << "contentUrl" << "value" << m_account->m_contentUrl << finalize,
//                                     find_and_upsert
//        );
//
//
//        LOG(TRACE) << endl << value.serialize();
//
//    }catch(std::exception &e){
//        LOG(ERROR) << endl << "There was an error while getting the endpoints for the account" << endl << e.what() <<endl;
//    }

}

void AcdApi::GetNewAccessToken() {



    //ignore ssl handshake due to self signed certificate

    try {
        //        std::cout << client.request(methods::POST, U("/"), params, U("application/x-www-form-urlencoded") ).get().to_string();
        cpr::Response response = cpr::Post(
                cpr::Url{urlForAccessToken(m_account)},
                cpr::Payload{{"refresh_token", m_account->m_clientRefreshToken}}
        );


//        LOG(TRACE) << endl << response.text;


        bsoncxx::document::value doc = bsoncxx::from_json(response.text);
        bsoncxx::document::view value =doc.view();

        auto accessToken = value["access_token"].get_utf8().value.to_string();
        auto expiry = value["expires_in"].get_int32().value;
        m_account->setNewAccessToken(accessToken, expiry);
        mongocxx::pool::entry conn = pool.acquire();
        mongocxx::database client = conn->database(DATABASENAME);
        mongocxx::collection settings = client[DATABASESETTINGS];

        settings.find_one_and_update(document{} << "name" << "accessToken" << finalize, document{} << "name" << "accessToken" << "value" << accessToken << finalize, find_and_upsert ) ;

        //        std::cout << client.request(req ).get().to_string();
    } catch (std::exception &exception) {
        LOG(ERROR) << exception.what();
    }
}

int AcdApi::parseMessage(std::string message){
    if(message.compare("Token has expired") == 0){
        LOG(TRACE) << endl << "Token has expired. Getting a new one" << endl;
        GetNewAccessToken();

        return ACD_TOKEN_EXPIRED;
    }
    LOG(TRACE) << endl << "Possible unhandled case" << endl << message <<endl;
    return ACD_SUCCESS;
}

void AcdApi::ConnectToDatabase(){

    mongocxx::pool::entry conn = pool.acquire();
    mongocxx::database client = conn->database(DATABASENAME);
    mongocxx::collection settings = client[DATABASESETTINGS];

    LOG(TRACE) << "Getting sync checkpoint";
    auto maybe_checkpoint = settings.find_one( document{} << "name" << "checkpoint" <<finalize);

    if(maybe_checkpoint){
        auto checkpoint = bsoncxx::document::view(*maybe_checkpoint);
        m_sync_checkpoint = checkpoint["value"].get_utf8().value.to_string();
        LOG(TRACE) << "Checkpoint Found!\t" << m_sync_checkpoint << endl;
    }

    auto maybe_access_token = settings.find_one( document{} << "name" << "accessToken" <<finalize);
    if(maybe_access_token){
        auto accessToken = bsoncxx::document::view(*maybe_access_token);
        m_account->setNewAccessToken(accessToken["value"].get_utf8().value.to_string(), 3600);

        LOG(TRACE) << "AccessToken Found!\t" << m_account->m_clientAccessToken << endl;
    }


}

AcdApi::AcdApi(Account *account): m_account(account){
    LOG(TRACE)  << "Instantiating a new  Api";
    account->api = this;
    upsert.upsert(true);
    find_and_upsert.upsert(true);
    ConnectToDatabase();



    SetEndPoints();
    Sync();

}

AcdApi::~AcdApi(){
    free(m_account);
}