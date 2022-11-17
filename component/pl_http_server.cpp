#include "pl_http_server.h"
#include "esp_check.h"
#include <map>

//==============================================================================

static const char* TAG = "pl_http_server";

//==============================================================================

namespace PL {

//==============================================================================

static std::map<int, HttpMethod> httpMethodMap {
  {HTTP_GET, HttpMethod::GET}, {HTTP_POST, HttpMethod::POST}, {HTTP_PUT, HttpMethod::PUT}, {HTTP_PATCH, HttpMethod::PATCH}, {HTTP_DELETE, HttpMethod::DELETE}
};

//==============================================================================

static std::map<uint16_t, std::string> httpStatusCodeMap {
  {100, "Continue"}, {101, "Switching Protocols"}, {102, "Processing"}, {103, "Early Hints"},
  
  {200, "OK"}, {201, "Created"}, {202, "Accepted"}, {203, "Non-Authoritative Information"}, {204, "No Content"}, {205, "Reset Content"},
  {206, "Partial Content"}, {207, "Multi-Status"}, {208, "Already Reported"}, {226, "IM Used"},
  
  {300, "Multiple Choices"}, {301, "Moved Permanently"}, {302, "Found"}, {303, "See Other"}, {304, "Not Modified"}, {305, "Use Proxy"},
  {307, "Temporary Redirect"}, {308, "Permanent Redirect"},
  
  {400, "Bad Request"}, {401, "Unauthorized"}, {402, "Payment Required"}, {403, "Forbidden"}, {404, "Not Found"}, {405, "Method Not Allowed"}, {406, "Not Acceptable"},
  {407, "Proxy Authentication Required"}, {408, "Request Timeout"}, {409, "Conflict"}, {410, "Gone"}, {411, "Length Required"}, {412, "Precondition Failed"},
  {413, "Payload Too Large"}, {414, "URI Too Long"}, {415, "Unsupported Media Type"}, {416, "Range Not Satisfiable"}, {417, "Expectation Failed"},
  {418, "I'm a teapot"}, {421, "Misdirected Request"}, {422, "Unprocessable Entity"}, {423, "Locked"}, {424, "Failed Dependency"}, {425, "Too Early"},
  {426, "Upgrade Required"}, {428, "Precondition Required"}, {429, "Too Many Requests"}, {431, "Request Header Fields Too Large"}, {451, "Unavailable For Legal Reasons"},
  
  {500, "Internal Server Error"}, {501, "Not Implemented"}, {502, "Bad Gateway"}, {503, "Service Unavailable"}, {504, "Gateway Timeout"}, {505, "HTTP Version Not Supported"},
  {506, "Variant Also Negotiates"}, {507, "Insufficient Storage"}, {508, "Loop Detected"}, {510, "Not Extended"}, {511, "Network Authentication Required"}
};

//==============================================================================

const std::string HttpServer::defaultHttpName = "HTTP Server";
const std::string HttpServer::defaultHttpsName = "HTTPS Server";
const TaskParameters HttpServer::defaultTaskParameters = {4096, tskIDLE_PRIORITY + 5, 0};

//==============================================================================

HttpServer::HttpServer (std::shared_ptr<Buffer> uriBuffer, std::shared_ptr<Buffer> headerBuffer) :
    requestEvent (*this), port (defaultHttpPort), uriBuffer (uriBuffer), headerBuffer (headerBuffer) {
  SetName (defaultHttpName);
}

//==============================================================================

HttpServer::HttpServer (size_t uriBufferSize, size_t headerBufferSize) :
  HttpServer (std::make_shared<Buffer> (uriBufferSize), std::make_shared<Buffer> (headerBufferSize)) {}

//==============================================================================

HttpServer::HttpServer (const char* certificate, const char* privateKey, std::shared_ptr<Buffer> uriBuffer, std::shared_ptr<Buffer> headerBuffer) :
    requestEvent (*this), port (defaultHttpsPort), uriBuffer (uriBuffer), headerBuffer (headerBuffer) {
  SetName (defaultHttpsName);
  https = true;
  this->serverCertificate = certificate;
  this->privateKey = privateKey;
}

//==============================================================================

HttpServer::HttpServer (const char* certificate, const char* privateKey, size_t uriBufferSize, size_t headerBufferSize) :
  HttpServer (certificate, privateKey, std::make_shared<Buffer> (uriBufferSize), std::make_shared<Buffer> (headerBufferSize)) {}

//==============================================================================

HttpServer::~HttpServer() {
  Disable();
}

//==============================================================================

esp_err_t HttpServer::Lock (TickType_t timeout) {
  esp_err_t error = mutex.Lock (timeout);
  if (error == ESP_OK)
    return ESP_OK;
  if (error == ESP_ERR_TIMEOUT && timeout == 0)
    return ESP_ERR_TIMEOUT;
  ESP_RETURN_ON_ERROR (error, TAG, "mutex lock failed");
  return ESP_OK;
}

//==============================================================================

esp_err_t HttpServer::Unlock() {
  ESP_RETURN_ON_ERROR (mutex.Unlock(), TAG, "mutex unlock failed");
  return ESP_OK;
}

//==============================================================================

esp_err_t HttpServer::Enable() {
  LockGuard lg (*this);
  if (enabled)
    return ESP_OK;

  serverConfig = HTTPD_SSL_CONFIG_DEFAULT();
  serverConfig.transport_mode = https ? HTTPD_SSL_TRANSPORT_SECURE : HTTPD_SSL_TRANSPORT_INSECURE;
  serverConfig.cacert_pem = https ? (const uint8_t*)serverCertificate : NULL;
  serverConfig.cacert_len = https ? strlen (serverCertificate) + 1 : 0;
  serverConfig.prvtkey_pem = https ? (const uint8_t*)privateKey : NULL;
  serverConfig.prvtkey_len = https ? strlen (privateKey) + 1 : 0;
  serverConfig.httpd.task_priority = taskParameters.priority;
  serverConfig.httpd.stack_size = taskParameters.stackDepth;
  serverConfig.httpd.core_id = taskParameters.coreId;
  serverConfig.httpd.server_port = serverConfig.httpd.ctrl_port = serverConfig.port_secure = serverConfig.port_insecure = port;
  serverConfig.httpd.backlog_conn = serverConfig.httpd.max_open_sockets = maxNumberOfClients;
  serverConfig.httpd.recv_wait_timeout = readTimeout * portTICK_PERIOD_MS / 1000 + 1;
  serverConfig.httpd.uri_match_fn = httpd_uri_match_wildcard;

  ESP_RETURN_ON_ERROR (httpd_ssl_start (&serverHandle, &serverConfig), TAG, "start failed");
  enabled = true;
  enabledEvent.Generate();

  httpd_uri_t requestHandlerInfo = {};
  requestHandlerInfo.uri = "*";
  requestHandlerInfo.handler = HandleRequest;
  requestHandlerInfo.user_ctx = this;
  http_method methods[] = {HTTP_GET, HTTP_POST, HTTP_PUT, HTTP_PATCH, HTTP_DELETE};

  for (uint32_t i = 0; i < sizeof(methods) / sizeof (http_method); i++) {
    requestHandlerInfo.method = methods[i];
    ESP_RETURN_ON_ERROR (httpd_register_uri_handler (serverHandle, &requestHandlerInfo), TAG, "register URI handler failed");
  }
  return ESP_OK;
}

//==============================================================================

esp_err_t HttpServer::Disable() {
  LockGuard lg (*this);
  if (!enabled)
    return ESP_OK;

  ESP_RETURN_ON_ERROR (httpd_unregister_uri (serverHandle, "*"), TAG, "unregister URI handler failed");
  httpd_ssl_stop (serverHandle);
  enabled = false;
  disabledEvent.Generate();
  return ESP_OK;
}

//==============================================================================

bool HttpServer::IsEnabled() {
  LockGuard lg (*this);
  return enabled;
}

//==============================================================================

uint16_t HttpServer::GetPort() {
  LockGuard lg (*this);
  return port;
}

//==============================================================================

esp_err_t HttpServer::SetPort (uint16_t port) {
  LockGuard lg (*this);
  this->port = port;
  ESP_RETURN_ON_ERROR (RestartIfEnabled(), TAG, "restart failed");
  return ESP_OK;
}

//==============================================================================

size_t HttpServer::GetMaxNumberOfClients() {
  LockGuard lg (*this);
  return maxNumberOfClients;;
}

//==============================================================================

esp_err_t HttpServer::SetMaxNumberOfClients (size_t maxNumberOfClients) {
  LockGuard lg (*this);
  this->maxNumberOfClients = maxNumberOfClients;
  ESP_RETURN_ON_ERROR (RestartIfEnabled(), TAG, "restart failed");
  return ESP_OK;
}
//==============================================================================

TickType_t HttpServer::GetReadTimeout() {
  LockGuard lg (*this);
  return readTimeout;
}

//==============================================================================

esp_err_t HttpServer::SetReadTimeout (TickType_t timeout) {
  LockGuard lg (*this);
  this->readTimeout = timeout;
  ESP_RETURN_ON_ERROR (RestartIfEnabled(), TAG, "restart failed");
  return ESP_OK;  
}

//==============================================================================

esp_err_t HttpServer::SetTaskParameters (const TaskParameters& taskParameters) {
  LockGuard lg (*this);
  this->taskParameters = taskParameters;
  ESP_RETURN_ON_ERROR (RestartIfEnabled(), TAG, "restart failed");
  return ESP_OK;
}

//==============================================================================

esp_err_t HttpServer::HandleRequest (httpd_req_t* req) {
  HttpServer& server = *(HttpServer*)req->user_ctx;
  auto uriBuffer = server.uriBuffer;
  auto headerBuffer = server.headerBuffer;
  LockGuard lgServer (server.mutex);
  LockGuard lgUriBuffer (*uriBuffer);
  LockGuard lgHeaderBuffer (*headerBuffer);
  Transaction transaction (server, req);

  if (strlen (req->uri) + 1 > uriBuffer->size) {
    transaction.WriteResponse (414);
    ESP_RETURN_ON_ERROR (ESP_ERR_INVALID_SIZE, TAG, "URI buffer is too small");
  } 
  strcpy ((char*)uriBuffer->data, req->uri);

  if (!headerBuffer->size) {
    transaction.WriteResponse (431);
    ESP_RETURN_ON_ERROR (ESP_ERR_INVALID_SIZE, TAG, "header buffer is too small");
  }

  // Not good, but there seem to be no other way to access all headers
  char* src = (char*)((uint8_t*)req->aux + sizeof (void*));
  char* srcEnd = src + CONFIG_HTTPD_MAX_REQ_HDR_LEN;
  char* dest = (char*)headerBuffer->data;
  char* destEnd = dest + headerBuffer->size - 1;

  while (src < srcEnd && *src) {
    while (src < srcEnd && *src) {
      *(dest++) = *src;
      if (dest >= destEnd) {
        transaction.WriteResponse (431);
        ESP_RETURN_ON_ERROR (ESP_ERR_INVALID_SIZE, TAG, "header buffer is too small");
      }
      if (*src != ':')
        src++;
      else {
        src++;
        for (; src < srcEnd && *src == ' '; src++); 
      }
    }
    *(dest++) = 0;
    src += 2;
  }
  server.requestHeaderDataEnd = server.responseHeaderDataEnd = dest;

  server.requestEvent.Generate (transaction);
  esp_err_t err = server.HandleRequest (transaction);
  if (err != ESP_OK)
    transaction.WriteResponse (500);
  ESP_RETURN_ON_ERROR (err, TAG, "handle request failed");
  return ESP_OK;
}

//==============================================================================

esp_err_t HttpServer::RestartIfEnabled() {
  if (!enabled)
    return ESP_OK;
  ESP_RETURN_ON_ERROR (Disable(), TAG, "disable failed");
  ESP_RETURN_ON_ERROR (Enable(), TAG, "enable failed");
  return ESP_OK;
}

//==============================================================================

HttpServer::Transaction::Transaction (HttpServer& server, httpd_req_t* req) :
  server (server), req (req), networkStream (std::make_shared<NetworkStream>(httpd_req_to_sockfd (req))) {}

//==============================================================================

esp_err_t HttpServer::Transaction::ReadRequestBody (void* dest, size_t size) {
  int res = httpd_req_recv (req, (char*)dest, size);
  if (res <= 0) {
    if (res == HTTPD_SOCK_ERR_TIMEOUT) {
      httpd_resp_send_408 (req);
      ESP_RETURN_ON_ERROR (ESP_ERR_TIMEOUT, TAG, "timeout");
    }
    ESP_RETURN_ON_ERROR (ESP_FAIL, TAG, "request receive failed"); 
  }
  return ESP_OK;
}

//==============================================================================

esp_err_t HttpServer::Transaction::WriteResponse (uint16_t statusCode, const void* body, size_t bodySize) {
  std::string status = std::to_string (statusCode) + " ";
  auto statusCodeIterator = httpStatusCodeMap.find (statusCode);
  if (statusCodeIterator != httpStatusCodeMap.end())
    status += statusCodeIterator->second;
  ESP_RETURN_ON_ERROR (httpd_resp_set_status (req, status.c_str()), TAG, "set status failed");
  ESP_RETURN_ON_ERROR (httpd_resp_send (req, (char*)body, bodySize), TAG, "response send failed");
  return ESP_OK;
}

//==============================================================================

std::shared_ptr<NetworkStream> HttpServer::Transaction::GetNetworkStream() {
  return networkStream;
}

//==============================================================================

HttpMethod HttpServer::Transaction::GetRequestMethod() {
  auto method = httpMethodMap.find (req->method);
  return method != httpMethodMap.end() ? method->second : HttpMethod::unknown;
}

//==============================================================================

const char* HttpServer::Transaction::GetRequestUri() {
  return (const char*)server.uriBuffer->data;
}

//==============================================================================

const char* HttpServer::Transaction::GetRequestHeader (const std::string& name) {
  char* end = server.requestHeaderDataEnd - name.size() - 2;
  for (char* ptr = (char*)server.headerBuffer->data; ptr < end; ) {
    if (strncasecmp (ptr, name.c_str(), name.size()) == 0 && *(ptr + name.size()) == ':')
      return ptr + name.size() + 1;

    for (; ptr < end && *ptr; ptr++);
    ptr++;
  }
  return NULL;
}

//==============================================================================

size_t HttpServer::Transaction::GetRequestBodySize() {
  return req->content_len;
}

//==============================================================================

esp_err_t HttpServer::Transaction::SetResponseHeader (const std::string& name, const std::string& value) {
  char*& responseHeaderDataEnd = server.responseHeaderDataEnd;
  ESP_RETURN_ON_FALSE (responseHeaderDataEnd - (char*)server.headerBuffer->data + name.size() + value.size() + 2 <= server.headerBuffer->size, \
                       ESP_ERR_INVALID_SIZE, TAG, "header buffer is too small");
  char* nameStr = responseHeaderDataEnd;
  memcpy (responseHeaderDataEnd, name.c_str(), name.size() + 1);
  responseHeaderDataEnd += name.size() + 1;
  char* valueStr = responseHeaderDataEnd;
  memcpy (responseHeaderDataEnd, value.c_str(), value.size() + 1);
  responseHeaderDataEnd += value.size() + 1;
  ESP_RETURN_ON_ERROR (httpd_resp_set_hdr (req, nameStr, valueStr), TAG, "set header failed");
  return ESP_OK;
}

//==============================================================================

}