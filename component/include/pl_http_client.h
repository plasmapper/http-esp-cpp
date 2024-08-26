#pragma once
#include "pl_common.h"
#include "pl_network.h"
#include "pl_http_types.h"
#include "esp_http_client.h"

//==============================================================================

namespace PL {

//==============================================================================

/// @brief HTTP/HTTPS client class
class HttpClient : public Lockable {
public:
  /// @brief Default HTTP port
  static const uint16_t defaultHttpPort = 80;
  /// @brief Default HTTPS port
  static const uint16_t defaultHttpsPort = 443;
  /// @brief Default read operation timeout in FreeRTOS ticks
  static const TickType_t defaultReadTimeout = 5000 / portTICK_PERIOD_MS;
  /// @brief Default header buffer size
  static const size_t defaultHeaderBufferSize = 1024;

  /// @brief Creates an HTTP client
  /// @param hostname hostname
  /// @param headerBuffer header buffer
  HttpClient(const std::string& hostname, std::shared_ptr<Buffer> headerBuffer);

  /// @brief Creates an HTTP client
  /// @param hostname hostname
  /// @param headerBufferSize header buffer size
  HttpClient(const std::string& hostname, size_t headerBufferSize = defaultHeaderBufferSize);

  /// @brief Creates an HTTPS client
  /// @param hostname hostname
  /// @param serverCertificate server certificate
  /// @param headerBuffer header buffer
  HttpClient(const std::string& hostname, const char* serverCertificate, std::shared_ptr<Buffer> headerBuffer);

  /// @brief Creates an HTTPS client
  /// @param hostname hostname
  /// @param serverCertificate server certificate
  /// @param headerBufferSize header buffer size
  HttpClient(const std::string& hostname, const char* serverCertificate, size_t headerBufferSize = defaultHeaderBufferSize);

  /// @brief Creates an HTTPS client
  /// @param hostname hostname
  /// @param crt_bundle_attach function pointer to esp_crt_bundle_attach
  /// @param headerBuffer header buffer
  HttpClient(const std::string& hostname, esp_err_t (*crt_bundle_attach)(void *conf), std::shared_ptr<Buffer> headerBuffer);

  /// @brief Creates an HTTPS client
  /// @param hostname hostname
  /// @param crt_bundle_attach function pointer to esp_crt_bundle_attach
  /// @param headerBufferSize header buffer size
  HttpClient(const std::string& hostname, esp_err_t (*crt_bundle_attach)(void *conf), size_t headerBufferSize = defaultHeaderBufferSize);

  ~HttpClient();
  HttpClient(const HttpClient&) = delete;
  HttpClient& operator=(const HttpClient&) = delete;

  esp_err_t Lock(TickType_t timeout = portMAX_DELAY) override;
  esp_err_t Unlock() override;

  /// @brief Initializes the client
  /// @return error code
  esp_err_t Initialize();

  /// @brief Writes the request headers
  /// @param method HTTP method
  /// @param uri URI
  /// @param bodySize body size
  /// @return error code
  esp_err_t WriteRequestHeaders(HttpMethod method, const std::string& uri, size_t bodySize);

  /// @brief Writes the request body
  /// @param src source
  /// @param size number of bytes to write
  /// @return error code
  esp_err_t WriteRequestBody(const void* src, size_t size);
  
  /// @brief Writes the request
  /// @param method HTTP method
  /// @param uri URI
  /// @param body body
  /// @return error code
  esp_err_t WriteRequest(HttpMethod method, const std::string& uri, const std::string& body);

  /// @brief Writes the request with an empty body
  /// @param method HTTP method
  /// @param uri URI
  /// @return 
  esp_err_t WriteRequest(HttpMethod method, const std::string& uri);

  /// @brief Reads the response headers
  /// @param statusCode status code
  /// @param bodySize body size
  /// @return error code
  esp_err_t ReadResponseHeaders(ushort& statusCode, size_t* bodySize);

  /// @brief Reads the response body
  /// @param dest destination (can be NULL)
  /// @param size number of bytes to read
  /// @return error code
  esp_err_t ReadResponseBody(void* dest, size_t size);

  /// @brief Disconnects from the server
  /// @return error code
  esp_err_t Disconnect();

  /// @brief Gets the remote port
  /// @return port
  uint16_t GetPort();

  /// @brief Sets the remote port
  /// @param port port
  /// @return error code
  esp_err_t SetPort(uint16_t port);

  /// @brief Gets the read operation timeout 
  /// @return timeout in FreeRTOS ticks
  TickType_t GetReadTimeout();

  /// @brief Sets the read operation timeout 
  /// @param timeout timeout in FreeRTOS ticks
  /// @return error code
  esp_err_t SetReadTimeout(TickType_t timeout);

  /// @brief Sets the request authentication scheme
  /// @param scheme authentication scheme
  /// @return error code
  esp_err_t SetAuthScheme(HttpAuthScheme scheme);

  /// @brief Sets the request authentication credentials
  /// @param username username
  /// @param password password
  /// @return error code
  esp_err_t SetAuthCredentials(const std::string& username, const std::string& password);

  /// @brief Sets the request header
  /// @param name header name
  /// @param value header value
  /// @return error code
  esp_err_t SetRequestHeader(const std::string& name, const std::string& value);

  /// @brief Deletes the request header
  /// @param name header name
  /// @return error code
  esp_err_t DeleteRequestHeader(const std::string& name);

  /// @brief Gets the response header value
  /// @param name header name
  /// @return header value (NULL if no such header in the request)
  const char* GetResponseHeader(const std::string& name);

private:
  Mutex mutex;
  std::string hostname;
  TickType_t readTimeout = defaultReadTimeout;
  std::shared_ptr<Buffer> headerBuffer;
  char* headerDataEnd;
  esp_http_client_config_t clientConfig = {};
  esp_http_client_handle_t clientHandle = NULL;

  static esp_err_t HandleResponse(esp_http_client_event_t* evt);
};

//==============================================================================

}