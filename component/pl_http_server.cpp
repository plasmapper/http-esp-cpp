#include "pl_http_server.h"
#include <map>

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
  return mutex.Lock (timeout);
}

//==============================================================================

esp_err_t HttpServer::Unlock() {
  return mutex.Unlock();
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

  PL_RETURN_ON_ERROR (httpd_ssl_start (&serverHandle, &serverConfig));
  enabled = true;
  enabledEvent.Generate();

  httpd_uri_t requestHandlerInfo = {};
  requestHandlerInfo.uri = "*";
  requestHandlerInfo.handler = HandleRequest;
  requestHandlerInfo.user_ctx = this;
  http_method methods[] = {HTTP_GET, HTTP_POST, HTTP_PUT, HTTP_PATCH, HTTP_DELETE};

  for (uint32_t i = 0; i < sizeof(methods) / sizeof (http_method); i++) {
    requestHandlerInfo.method = methods[i];
    PL_RETURN_ON_ERROR (httpd_register_uri_handler (serverHandle, &requestHandlerInfo));
  }
  return ESP_OK;
}

//==============================================================================

esp_err_t HttpServer::Disable() {
  LockGuard lg (*this);
  if (!enabled)
    return ESP_OK;

  PL_RETURN_ON_ERROR (httpd_unregister_uri (serverHandle, "*"));
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
  return RestartIfEnabled();
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
  return RestartIfEnabled();
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
  return RestartIfEnabled();  
}

//==============================================================================

esp_err_t HttpServer::SetTaskParameters (const TaskParameters& taskParameters) {
  LockGuard lg (*this);
  this->taskParameters = taskParameters;
  return RestartIfEnabled();
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
    return ESP_ERR_INVALID_SIZE;
  } 
  strcpy ((char*)uriBuffer->data, req->uri);

  // Not good, but there seem to be no other way to access all headers
  char* src = (char*)((uint8_t*)req->aux + sizeof (void*));
  char* srcEnd = src + CONFIG_HTTPD_MAX_REQ_HDR_LEN;
  char* dest = (char*)headerBuffer->data;
  char* destEnd = dest + headerBuffer->size - 1;
  if (dest >= destEnd) {
    transaction.WriteResponse (431);
    return ESP_ERR_INVALID_SIZE;
  }

  while (src < srcEnd && *src) {
    while (src < srcEnd && *src) {
      *(dest++) = *src;
      if (dest >= destEnd) {
        transaction.WriteResponse (431);
        return ESP_ERR_INVALID_SIZE;
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
  return err;
}

//==============================================================================

esp_err_t HttpServer::RestartIfEnabled() {
  if (!enabled)
    return ESP_OK;
  PL_RETURN_ON_ERROR (Disable());
  return Enable();
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
    } 
    return ESP_FAIL;
  }
  return ESP_OK;
}

//==============================================================================

esp_err_t HttpServer::Transaction::WriteResponse (uint16_t statusCode, const void* body, size_t bodySize) {
  std::string status = std::to_string (statusCode) + " ";
  auto statusCodeIterator = httpStatusCodeMap.find (statusCode);
  if (statusCodeIterator != httpStatusCodeMap.end())
    status += statusCodeIterator->second;
  PL_RETURN_ON_ERROR (httpd_resp_set_status (req, status.c_str()));

  return httpd_resp_send (req, (char*)body, bodySize);
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
  if (responseHeaderDataEnd - (char*)server.headerBuffer->data + name.size() + value.size() + 2 > server.headerBuffer->size)
    return ESP_ERR_INVALID_SIZE;
  char* nameStr = responseHeaderDataEnd;
  memcpy (responseHeaderDataEnd, name.c_str(), name.size() + 1);
  responseHeaderDataEnd += name.size() + 1;
  char* valueStr = responseHeaderDataEnd;
  memcpy (responseHeaderDataEnd, value.c_str(), value.size() + 1);
  responseHeaderDataEnd += value.size() + 1;
  return httpd_resp_set_hdr (req, nameStr, valueStr);
}

//==============================================================================

}