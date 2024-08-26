#include "pl_http.h"

//==============================================================================

class HttpServer : public PL::HttpServer {
public:
  using PL::HttpServer::HttpServer;

protected:
  esp_err_t HandleRequest(PL::HttpServerTransaction& transaction) override;
};

//==============================================================================

void TestHttpServer();
void TestHttpsServer();