#include "pl_http_client.h"
#include "esp_check.h"
#include <map>

//==============================================================================

static const char* TAG = "pl_http_client";

//==============================================================================

namespace PL {

//==============================================================================

static std::map<HttpMethod, esp_http_client_method_t> httpMethodMap {
  {HttpMethod::GET, HTTP_METHOD_GET}, {HttpMethod::POST, HTTP_METHOD_POST}, {HttpMethod::PUT, HTTP_METHOD_PUT}, {HttpMethod::PATCH, HTTP_METHOD_PATCH}, {HttpMethod::DELETE, HTTP_METHOD_DELETE}
};

static std::map<HttpAuthScheme, esp_http_client_auth_type_t> httpAuthSchemeMap {
  {HttpAuthScheme::none, HTTP_AUTH_TYPE_NONE}, {HttpAuthScheme::basic, HTTP_AUTH_TYPE_BASIC}, {HttpAuthScheme::digest, HTTP_AUTH_TYPE_DIGEST}
};

//==============================================================================

HttpClient::HttpClient(const std::string& hostname, std::shared_ptr<Buffer> headerBuffer) : hostname(hostname), headerBuffer(headerBuffer) {
  headerDataEnd = (char*)headerBuffer->data;
  clientConfig.host = hostname.c_str();
  clientConfig.path = "/";
  clientConfig.event_handler = HandleResponse;
  clientConfig.user_data = this;
  clientConfig.transport_type = HTTP_TRANSPORT_OVER_TCP;
  clientConfig.port = defaultHttpPort;
}

//==============================================================================

HttpClient::HttpClient(const std::string& hostname, size_t headerBufferSize) : HttpClient(hostname, std::make_shared<Buffer>(headerBufferSize)) {}

//==============================================================================

HttpClient::HttpClient(const std::string& hostname, const char* certificate, std::shared_ptr<Buffer> headerBuffer) :
    HttpClient(hostname, headerBuffer) {
  clientConfig.cert_pem = certificate;
  clientConfig.cert_len = strlen(certificate) + 1;
  clientConfig.transport_type = HTTP_TRANSPORT_OVER_SSL;
  clientConfig.port = defaultHttpsPort;
}

//==============================================================================

HttpClient::HttpClient(const std::string& hostname, const char* certificate, size_t headerBufferSize) :
  HttpClient(hostname, certificate, std::make_shared<Buffer>(headerBufferSize)) {}

//==============================================================================

HttpClient::HttpClient(const std::string& hostname, esp_err_t (*crt_bundle_attach)(void *conf), std::shared_ptr<Buffer> headerBuffer) :
    HttpClient(hostname, headerBuffer) {
  clientConfig.crt_bundle_attach = crt_bundle_attach;
  clientConfig.transport_type = HTTP_TRANSPORT_OVER_SSL;
  clientConfig.port = defaultHttpsPort;
}

//==============================================================================

HttpClient::HttpClient(const std::string& hostname, esp_err_t (*crt_bundle_attach)(void *conf), size_t headerBufferSize) :
  HttpClient(hostname, crt_bundle_attach, std::make_shared<Buffer>(headerBufferSize)) {}

//==============================================================================

HttpClient::~HttpClient() {
  if (clientHandle)
    esp_http_client_cleanup(clientHandle);
}

//==============================================================================

esp_err_t HttpClient::Lock(TickType_t timeout) {
  esp_err_t error = mutex.Lock(timeout);
  if (error == ESP_OK)
    return ESP_OK;
  if (error == ESP_ERR_TIMEOUT && timeout == 0)
    return ESP_ERR_TIMEOUT;
  ESP_RETURN_ON_ERROR(error, TAG, "mutex lock failed");
  return ESP_OK;
}

//==============================================================================

esp_err_t HttpClient::Unlock() {
  ESP_RETURN_ON_ERROR(mutex.Unlock(), TAG, "mutex unlock failed");
  return ESP_OK;
}

//==============================================================================

esp_err_t HttpClient::Initialize() {
  LockGuard lg(*this);
  if (clientHandle)
    return ESP_OK;
  clientHandle = esp_http_client_init(&clientConfig);
  ESP_RETURN_ON_FALSE(clientHandle, ESP_FAIL, TAG, "init failed");
  return ESP_OK;
}

//==============================================================================

esp_err_t HttpClient::WriteRequestHeaders(HttpMethod method, const std::string& uri, size_t bodySize) {
  LockGuard lg(*this);
  ESP_RETURN_ON_FALSE(clientHandle, ESP_ERR_INVALID_STATE, TAG, "HTTP client is not initialized");

  auto espMethod = httpMethodMap.find(method);
  ESP_RETURN_ON_FALSE(espMethod != httpMethodMap.end(), ESP_ERR_INVALID_ARG, TAG, "invalid HTTP method");

  ESP_RETURN_ON_ERROR(esp_http_client_flush_response(clientHandle, NULL), TAG, "flush response failed");
  ESP_RETURN_ON_ERROR(esp_http_client_set_method(clientHandle, espMethod->second), TAG, "set method failed");
  ESP_RETURN_ON_ERROR(esp_http_client_set_url(clientHandle, uri.c_str()), TAG, "set URL failed");
  ESP_RETURN_ON_ERROR(esp_http_client_open(clientHandle, bodySize), TAG, "open failed");
  return ESP_OK;
}

//==============================================================================

esp_err_t HttpClient::WriteRequestBody(const void* src, size_t size) {
  LockGuard lg(*this);
  ESP_RETURN_ON_FALSE(clientHandle, ESP_ERR_INVALID_STATE, TAG, "HTTP client is not initialized");
  ESP_RETURN_ON_FALSE(esp_http_client_write(clientHandle, (char*)src, size) >= 0, ESP_FAIL, TAG, "write failed");
  return ESP_OK;
}

//==============================================================================

esp_err_t HttpClient::WriteRequest(HttpMethod method, const std::string& uri, const std::string& body) {
  ESP_RETURN_ON_ERROR(WriteRequestHeaders(method, uri, body.size()), TAG, "write request headers failed");
  ESP_RETURN_ON_ERROR(WriteRequestBody(body.data(), body.size()), TAG, "write request body failed");
  return ESP_OK;
}

//==============================================================================

esp_err_t HttpClient::WriteRequest(HttpMethod method, const std::string& uri) {
  return WriteRequestHeaders(method, uri, 0);
}

//==============================================================================

esp_err_t HttpClient::ReadResponseHeaders(ushort& statusCode, size_t* bodySize) {
  LockGuard lg(*this, *headerBuffer);
  ESP_RETURN_ON_FALSE(clientHandle, ESP_ERR_INVALID_STATE, TAG, "HTTP client is not initialized");

  ESP_RETURN_ON_ERROR(esp_http_client_set_timeout_ms(clientHandle, readTimeout * portTICK_PERIOD_MS), TAG, "HTTP client set timeout failed");
  headerDataEnd = (char*)headerBuffer->data;
  
  int64_t tempResponseBodySize = esp_http_client_fetch_headers(clientHandle);
  ESP_RETURN_ON_FALSE(tempResponseBodySize >= 0, ESP_FAIL, TAG, "fetch headers failed");

  statusCode = esp_http_client_get_status_code(clientHandle);
  if (statusCode == 401)
    esp_http_client_add_auth(clientHandle);
  if (bodySize)
    *bodySize = tempResponseBodySize;
    
  return ESP_OK;
}

//==============================================================================

esp_err_t HttpClient::ReadResponseBody(void* dest, size_t size) {
  LockGuard lg(*this);
  ESP_RETURN_ON_FALSE(clientHandle, ESP_ERR_INVALID_STATE, TAG, "HTTP client is not initialized");
  ESP_RETURN_ON_ERROR(esp_http_client_set_timeout_ms(clientHandle, readTimeout * portTICK_PERIOD_MS), TAG, "set timeout failed");
  ESP_RETURN_ON_FALSE(esp_http_client_read(clientHandle, (char*)dest, size) == size, ESP_FAIL, TAG, "read failed");
  return ESP_OK;
}

//==============================================================================

esp_err_t HttpClient::Disconnect() {
  ESP_RETURN_ON_ERROR(esp_http_client_close(clientHandle), TAG, "close failed");
  return ESP_OK;
}

//==============================================================================

uint16_t HttpClient::GetPort() {
  LockGuard lg(*this);
  return clientConfig.port;
}

//==============================================================================

esp_err_t HttpClient::SetPort(uint16_t port) {
  LockGuard lg(*this);
  clientConfig.port = port;

  if (!clientHandle)
    return ESP_OK;
  esp_http_client_handle_t tempHandle = clientHandle;
  clientHandle = NULL;
  ESP_RETURN_ON_ERROR(esp_http_client_cleanup(tempHandle), TAG, "cleanup failed");
  ESP_RETURN_ON_ERROR(Initialize(), TAG, "initialize failed");
  return ESP_OK; 
}

//==============================================================================

TickType_t HttpClient::GetReadTimeout() {
  LockGuard lg(*this);
  return readTimeout;
}

//==============================================================================

esp_err_t HttpClient::SetReadTimeout(TickType_t timeout) {
  LockGuard lg(*this);
  this->readTimeout = timeout;
  return ESP_OK;
}

//==============================================================================

esp_err_t HttpClient::SetAuthScheme(HttpAuthScheme scheme) {
  LockGuard lg(*this);
  ESP_RETURN_ON_FALSE(clientHandle, ESP_ERR_INVALID_STATE, TAG, "HTTP client is not initialized");
  auto espAuthScheme = httpAuthSchemeMap.find(scheme);
  ESP_RETURN_ON_FALSE(espAuthScheme != httpAuthSchemeMap.end(), ESP_ERR_INVALID_ARG, TAG, "invalid authentication scheme");
  ESP_RETURN_ON_ERROR(esp_http_client_set_authtype(clientHandle, espAuthScheme->second), TAG, "set authentication scheme failed");
  return ESP_OK;
}

//==============================================================================

esp_err_t HttpClient::SetAuthCredentials(const std::string& username, const std::string& password) {
  LockGuard lg(*this);
  ESP_RETURN_ON_FALSE(clientHandle, ESP_ERR_INVALID_STATE, TAG, "HTTP client is not initialized");
  ESP_RETURN_ON_ERROR(esp_http_client_set_username(clientHandle, username.c_str()), TAG, "set username failed");
  ESP_RETURN_ON_ERROR(esp_http_client_set_password(clientHandle, password.c_str()), TAG, "set password failed");
  return ESP_OK;
}
 
//==============================================================================

esp_err_t HttpClient::SetRequestHeader(const std::string& name, const std::string& value) {
  LockGuard lg(*this);
  ESP_RETURN_ON_FALSE(clientHandle, ESP_ERR_INVALID_STATE, TAG, "HTTP client is not initialized");
  ESP_RETURN_ON_ERROR(esp_http_client_set_header(clientHandle, name.c_str(), value.c_str()), TAG, "set header failed");
  return ESP_OK;
}

//==============================================================================

esp_err_t HttpClient::DeleteRequestHeader(const std::string& name) {
  LockGuard lg(*this);
  ESP_RETURN_ON_FALSE(clientHandle, ESP_ERR_INVALID_STATE, TAG, "HTTP client is not initialized");
  ESP_RETURN_ON_ERROR(esp_http_client_delete_header(clientHandle, name.c_str()), TAG, "delete header failed");
  return ESP_OK;
}

//==============================================================================

const char* HttpClient::GetResponseHeader(const std::string& name) {
  LockGuard lg(*this);
  char* end = headerDataEnd - name.size() - 2;
  for (char* ptr = (char*)headerBuffer->data; ptr < end; ) {
    if (strncasecmp(ptr, name.c_str(), name.size()) == 0 && *(ptr + name.size()) == ':')
      return ptr + name.size() + 1;
    for (; ptr < end && *ptr; ptr++);
    ptr++;
  }
  return NULL;
}

//==============================================================================

esp_err_t HttpClient::HandleResponse(esp_http_client_event_t* evt) {
  HttpClient& client = *(HttpClient*)evt->user_data;
  auto headerBuffer = client.headerBuffer;
  char*& headerDataEnd = client.headerDataEnd;

  if (evt->event_id == HTTP_EVENT_ON_HEADER) {
    size_t headerNameSize = strlen(evt->header_key);
    size_t headerValueSize = strlen(evt->header_value);

    ESP_RETURN_ON_FALSE(headerDataEnd - (char*)headerBuffer->data + headerNameSize + headerValueSize + 2 <= headerBuffer->size, ESP_ERR_INVALID_SIZE, TAG, "header buffer is too small");
    memcpy(headerDataEnd, evt->header_key, headerNameSize);
    headerDataEnd += headerNameSize;
    *(headerDataEnd++) = ':';
    memcpy(headerDataEnd, evt->header_value, headerValueSize + 1);
    headerDataEnd += headerValueSize + 1;
  }

  return ESP_OK;
}

//==============================================================================

};