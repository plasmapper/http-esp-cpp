#pragma once
#include "pl_common.h"
#include "pl_network.h"
#include "pl_http_server_transaction.h"
#include "esp_https_server.h"

//==============================================================================

namespace PL {

//==============================================================================

/// @brief HTTP/HTTPS server class
class HttpServer : public NetworkServer {
public:
  /// @brief Default HTTP server name
  static const std::string defaultHttpName;
  /// @brief Default HTTPS server name
  static const std::string defaultHttpsName;
  /// @brief Default HTTP port
  static const uint16_t defaultHttpPort = 80;
  /// @brief Default HTTPS port
  static const uint16_t defaultHttpsPort = 443;
  /// @brief Default maximum number of server clients
  static const int defaultMaxNumberOfClients = 1;
  /// @brief Default read operation timeout in FreeRTOS ticks
  static const TickType_t defaultReadTimeout = 5000 / portTICK_PERIOD_MS;
  /// @brief Default server task parameters
  static const TaskParameters defaultTaskParameters;
  /// @brief Default URI buffer size
  static const size_t defaultUriBufferSize = 512;
  /// @brief Default header buffer size
  static const size_t defaultHeaderBufferSize = 1024;

  Event<HttpServer, HttpServerTransaction&> requestEvent;
  
  /// @brief Creates an HTTP server with shared transaction buffer
  /// @param uriBuffer URI buffer
  /// @param headerBuffer header buffer
  HttpServer(std::shared_ptr<Buffer> uriBuffer, std::shared_ptr<Buffer> headerBuffer);

  /// @brief Creates an HTTP server and allocate transaction buffer
  /// @param uriBufferSize URI buffer size
  /// @param headerBufferSize header buffer size
  HttpServer(size_t uriBufferSize = defaultUriBufferSize, size_t headerBufferSize = defaultHeaderBufferSize);

  /// @brief Creates an HTTPS server with shared transaction buffer
  /// @param certificate certificate
  /// @param privateKey private key
  /// @param uriBuffer URI buffer
  /// @param headerBuffer header buffer
  HttpServer(const char* certificate, const char* privateKey, std::shared_ptr<Buffer> uriBuffer, std::shared_ptr<Buffer> headerBuffer);

  /// @brief Creates an HTTPS server and allocate transaction buffer
  /// @param certificate certificate
  /// @param privateKey private key
  /// @param uriBufferSize URI buffer size
  /// @param headerBufferSize header buffer size
  HttpServer(const char* certificate, const char* privateKey, size_t uriBufferSize = defaultUriBufferSize, size_t headerBufferSize = defaultHeaderBufferSize);

  ~HttpServer();
  HttpServer(const HttpServer&) = delete;
  HttpServer& operator=(const HttpServer&) = delete;

  esp_err_t Lock(TickType_t timeout = portMAX_DELAY) override;
  esp_err_t Unlock() override;

  esp_err_t Enable() override;
  esp_err_t Disable() override;

  bool IsEnabled() override;

  uint16_t GetPort() override;
  esp_err_t SetPort(uint16_t port) override;

  size_t GetMaxNumberOfClients() override;
  esp_err_t SetMaxNumberOfClients(size_t maxNumberOfClients) override;

  /// @brief Gets the read operation timeout 
  /// @return timeout in FreeRTOS ticks
  TickType_t GetReadTimeout();

  /// @brief Sets the read operation timeout 
  /// @param timeout timeout in FreeRTOS ticks
  /// @return error code
  esp_err_t SetReadTimeout(TickType_t timeout);

  /// @brief Sets the server task parameters
  /// @param taskParameters task parameters
  /// @return error code
  esp_err_t SetTaskParameters(const TaskParameters& taskParameters);

protected:
  /// @brief Handles the HTTP request
  /// @param clientStream 
  /// @param request request
  /// @param response response
  /// @return error code
  virtual esp_err_t HandleRequest(HttpServerTransaction& transaction) = 0;

private:
  Mutex mutex;
  bool enabled = false;
  uint16_t port;
  int maxNumberOfClients = defaultMaxNumberOfClients;
  TickType_t readTimeout = defaultReadTimeout;
  TaskParameters taskParameters = defaultTaskParameters;
  std::shared_ptr<Buffer> uriBuffer;
  std::shared_ptr<Buffer> headerBuffer;
  char* requestHeaderDataEnd;
  char* responseHeaderDataEnd;
  bool https = false;
  const char* serverCertificate = NULL;
  const char* privateKey = NULL;
  httpd_ssl_config_t serverConfig;
  httpd_handle_t serverHandle = NULL;
  
  static esp_err_t HandleRequest(httpd_req_t* req);
  esp_err_t RestartIfEnabled();

  class Transaction : public HttpServerTransaction {
  public:
    Transaction(HttpServer& server, httpd_req_t* req);

    esp_err_t ReadRequestBody(void* dest, size_t size) override;
    using HttpServerTransaction::WriteResponse;
    esp_err_t WriteResponse(uint16_t statusCode, const void* body, size_t bodySize) override;

    std::shared_ptr<NetworkStream> GetNetworkStream() override;
    HttpMethod GetRequestMethod() override;
    const char* GetRequestUri() override;
    const char* GetRequestHeader(const std::string& name) override;
    size_t GetRequestBodySize() override;

    esp_err_t SetResponseHeader(const std::string& name, const std::string& value) override;

  private:
    HttpServer& server;
    httpd_req_t* req;
    std::shared_ptr<NetworkStream> networkStream;
  };
};

//==============================================================================

}