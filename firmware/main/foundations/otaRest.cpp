#include <esp_log.h>
#include "webserver.h"
#include "ota.h"
#include "reboot.h"

static const char tag[] = "otaRest";

class OTAWebHandler : public WebServerHandler {
protected:
    class DLContext {
    public:
        bool image;
        OTA* ota;
        bool reply;
    
        DLContext() : image(false), ota(NULL), reply(false){};
        virtual ~DLContext(){
            if (ota){
                end(false);
            }
        };

        void start(OTA* in){
            ota = in;
        };
        OTARESULT end(bool needCommit){
            OTARESULT rc = ::endOTA(ota, needCommit);
            ota = NULL;
            return rc;
        }
    };
    const char* vkeypath;
    bool needDigestAuth;
    std::function<void(OTAPHASE)>* handler;

public:
    OTAWebHandler(const char* vkey, bool auth,
                  std::function<void(OTAPHASE)>* complete);
    virtual ~OTAWebHandler();

    virtual bool needDigestAuthentication(HttpRequest& req) override;

    virtual void receiveRequest(WebServerConnection& connection) override;

    virtual void beginMultipart(WebServerConnection& connection) override;
    virtual void endMultipart(WebServerConnection& connection) override;

    virtual void beginMultipartData(WebServerConnection& connection,
                                    const char* key) override;
    virtual void updateMultipartData(WebServerConnection& connection,
                                     const char* key,
                                     const void* data, size_t length) override;
    virtual void endMultipartData(WebServerConnection& connection) override;
};

static void buildInvalidResp(HttpResponse* resp){
    resp->setHttpStatus(HttpResponse::RESP_403_Forbidden);
    resp->addHeader(WebString("Content-Type"), WebString("text/plain"));
    resp->setBody("Forbidden");
    resp->close();
}

OTAWebHandler::OTAWebHandler(const char* vkey, bool auth,
                             std::function<void(OTAPHASE)>* handler) :
    vkeypath(vkey), needDigestAuth(auth),handler(handler){
}

OTAWebHandler::~OTAWebHandler(){
}
    
bool OTAWebHandler::needDigestAuthentication(HttpRequest& req){
    return needDigestAuth;
}

void OTAWebHandler::receiveRequest(WebServerConnection& connection){
    buildInvalidResp(connection.response());
}

void OTAWebHandler::beginMultipart(WebServerConnection& connection){
    auto req = connection.request();
    auto resp = connection.response();
    if (req->method() != HttpRequest::MethodPost){
        buildInvalidResp(resp);
        return;
    }

    auto ctx = new DLContext();

    ESP_LOGI(tag, "OTA download request");
    auto imageSize =
        req->header().at(WebString("X-OTA-Image-Size")).intvalue();
    ESP_LOGI(tag, "image size: %d", imageSize);
    
    if (handler){
        (*handler)(OTA_BEGIN);
    }

    OTA* ota = NULL;
    if (startOTA(vkeypath, imageSize, &ota) != OTA_SUCCEED){
        ESP_LOGE(tag, "downloading in parallel");
        resp->setHttpStatus(HttpResponse::RESP_500_InternalServerError);
        resp->addHeader(WebString("Content-Type"), WebString("text/plain"));
        resp->setBody("firmware downloading is proceeding in parallel");
        resp->close();
        delete ctx;

        return;
    }
    ctx->start(ota);
    connection.setUserContext(ctx);
}

void OTAWebHandler::endMultipart(WebServerConnection& connection){
    auto resp = connection.response();
    auto ctx = (DLContext*)connection.getUserContext();
    if (ctx && !ctx->reply){
        ESP_LOGE(tag, "no image part");
        
        resp->setHttpStatus(HttpResponse::RESP_500_InternalServerError);
        resp->addHeader(WebString("Content-Type"), WebString("text/plain"));
        resp->setBody("no firmware image contains in a request");
        resp->close();
        ctx->end(false);
    }
    connection.setUserContext(NULL);
    delete ctx;
}

void OTAWebHandler::beginMultipartData(WebServerConnection& connection,
                                       const char* key){
    auto ctx = (DLContext*)connection.getUserContext();
    if (ctx && ctx->ota && strcmp(key, "image") == 0){
        ESP_LOGI(tag, "begin update firmware");
        ctx->image = true;
    }
}

void OTAWebHandler::updateMultipartData(WebServerConnection& connection,
                                        const char* key,
                                        const void* data, size_t length){
    auto ctx = (DLContext*)connection.getUserContext();
    if (ctx && ctx->ota && ctx->image){
        auto rc = ctx->ota->addDataFlagment(data, length);
        if (rc != OTA_SUCCEED){
            ESP_LOGE(tag, "update data failed");
            auto resp = connection.response();
            resp->setHttpStatus(HttpResponse::RESP_500_InternalServerError);
            resp->addHeader(WebString("Content-Type"),
                            WebString("text/plain"));
            resp->setBody("firmware image might corrupted");
            resp->close();
            ctx->end(false);
            ctx->reply = true;
        }
    }
}

void OTAWebHandler::endMultipartData(WebServerConnection& connection){
    auto ctx = (DLContext*)connection.getUserContext();
    if (ctx && ctx->ota && ctx->image){
        ESP_LOGI(tag, "end updating firmware");
        auto resp = connection.response();
        auto rc = ctx->end(true);
        if (rc == OTA_SUCCEED){
            resp->setHttpStatus(HttpResponse::RESP_200_OK);
            resp->addHeader(WebString("Content-Type"),
                            WebString("text/plain"));
            resp->setBody("firmware updating finished");
            if (handler){
                (*handler)(OTA_END);
            }
            rebootIn(2000);
        }else{
            ESP_LOGE(tag, "commit failed");
            resp->setHttpStatus(HttpResponse::RESP_500_InternalServerError);
            resp->addHeader(WebString("Content-Type"),
                            WebString("text/plain"));
            resp->setBody("update data failed");
            if (handler){
                (*handler)(OTA_ERROR);
            }
        }
        resp->close();
        ctx->reply = true;
    }
}

WebServerHandler* getOTAWebHandler(const char* vkeypath,
                                   bool needDigestAuth,
                                   std::function<void(OTAPHASE)>* handler){
    return new OTAWebHandler(vkeypath, needDigestAuth, handler);
}
