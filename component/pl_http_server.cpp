#include "pl_http_server.h"
#include "esp_check.h"
#include <map>
#include <vector>

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

const TaskParameters HttpServer::defaultTaskParameters = {4096, tskIDLE_PRIORITY + 5, 0};

//==============================================================================

HttpServer::HttpServer(std::shared_ptr<Buffer> headerBuffer) : requestEvent(*this), port(defaultHttpPort), headerBuffer(headerBuffer) {
  SetName(defaultHttpName);
}

//==============================================================================

HttpServer::HttpServer(size_t headerBufferSize) : HttpServer(std::make_shared<Buffer>(headerBufferSize)) {}

//==============================================================================

HttpServer::HttpServer(const char* certificate, const char* privateKey, std::shared_ptr<Buffer> headerBuffer) : requestEvent(*this), port(defaultHttpsPort), headerBuffer(headerBuffer) {
  SetName(defaultHttpsName);
  https = true;
  this->serverCertificate = certificate;
  this->privateKey = privateKey;
}

//==============================================================================

HttpServer::HttpServer(const char* certificate, const char* privateKey, size_t headerBufferSize) :
  HttpServer(certificate, privateKey, std::make_shared<Buffer>(headerBufferSize)) {}

//==============================================================================

HttpServer::~HttpServer() {
  if (enabled) {
    ESP_LOGE(TAG, "StopTask was not called by the derived class destructor");
    abort();
  }
}

//==============================================================================

esp_err_t HttpServer::Lock(TickType_t timeout) {
  esp_err_t error = mutex.Lock(timeout);
  if (error != ESP_OK && (error != ESP_ERR_TIMEOUT || timeout != 0))
    ESP_LOGE(TAG, "mutex lock failed");
  return error;
}

//==============================================================================

esp_err_t HttpServer::Unlock() {
  ESP_RETURN_ON_ERROR(mutex.Unlock(), TAG, "mutex unlock failed");
  return ESP_OK;
}

//==============================================================================

esp_err_t HttpServer::Enable() {
  LockGuard lg(*this);
  if (handlingRequest) {
    enableFromRequest = true;
    return ESP_OK;
  }
  if (enabled)
    return ESP_OK;

  serverConfig = HTTPD_SSL_CONFIG_DEFAULT();
  serverConfig.transport_mode = https ? HTTPD_SSL_TRANSPORT_SECURE : HTTPD_SSL_TRANSPORT_INSECURE;
  serverConfig.servercert = (https && serverCertificate) ? (const uint8_t*)serverCertificate : NULL;
  serverConfig.servercert_len = (https && serverCertificate) ? strlen(serverCertificate) + 1 : 0;
  serverConfig.prvtkey_pem = (https && privateKey) ? (const uint8_t*)privateKey : NULL;
  serverConfig.prvtkey_len = (https && privateKey) ? strlen(privateKey) + 1 : 0;
  serverConfig.httpd.task_priority = taskParameters.priority;
  serverConfig.httpd.stack_size = taskParameters.stackDepth;
  serverConfig.httpd.core_id = taskParameters.coreId;
  serverConfig.httpd.server_port = serverConfig.httpd.ctrl_port = serverConfig.port_secure = serverConfig.port_insecure = port;
  serverConfig.httpd.backlog_conn = serverConfig.httpd.max_open_sockets = maxNumberOfClients;
  serverConfig.httpd.recv_wait_timeout = readTimeout == portMAX_DELAY ? UINT16_MAX : readTimeout * portTICK_PERIOD_MS / 1000 + 1;
  serverConfig.httpd.send_wait_timeout = writeTimeout == portMAX_DELAY ? UINT16_MAX : writeTimeout * portTICK_PERIOD_MS / 1000 + 1;
  serverConfig.httpd.uri_match_fn = httpd_uri_match_wildcard;

  ESP_RETURN_ON_ERROR(httpd_ssl_start(&serverHandle, &serverConfig), TAG, "start failed");

  httpd_uri_t requestHandlerInfo = {};
  requestHandlerInfo.uri = "*";
  requestHandlerInfo.handler = HandleRequest;
  requestHandlerInfo.user_ctx = this;
  http_method methods[] = {HTTP_GET, HTTP_POST, HTTP_PUT, HTTP_PATCH, HTTP_DELETE};

  for (uint32_t i = 0; i < sizeof(methods) / sizeof(http_method); i++) {
    requestHandlerInfo.method = methods[i];
    esp_err_t error = httpd_register_uri_handler(serverHandle, &requestHandlerInfo);
    if (error != ESP_OK) {
      httpd_ssl_stop(serverHandle);
      ESP_RETURN_ON_ERROR(error, TAG, "register URI handler failed");
    }
  }

  enabled = true;
  enabledEvent.Generate();
  return ESP_OK;
}

//==============================================================================

esp_err_t HttpServer::Disable() {
  LockGuard lg(*this);
  if (handlingRequest) {
    enableFromRequest = false;
    disableFromRequest = true;
    return ESP_OK;
  }
  if (!enabled)
    return ESP_OK;

  ESP_RETURN_ON_ERROR(StopTask(), TAG, "stop task failed");
  return ESP_OK;
}

//==============================================================================

esp_err_t HttpServer::StopTask() {
  if (handlingRequest) {
    ESP_LOGE(TAG, "stop task called from the server task itself");
    abort();
  }
  LockGuard lg(*this);
  if (!enabled)
    return ESP_OK;

  if (httpd_unregister_uri(serverHandle, "*") != ESP_OK)
    ESP_LOGE(TAG, "unregister URI failed");
  ESP_RETURN_ON_ERROR(httpd_ssl_stop(serverHandle), TAG, "stop failed");
  serverHandle = NULL;
  enabled = false;
  disabledEvent.Generate();
  return ESP_OK;
}

//==============================================================================

bool HttpServer::IsEnabled() {
  LockGuard lg(*this);
  return enabled;
}

//==============================================================================

uint16_t HttpServer::GetPort() {
  LockGuard lg(*this);
  return port;
}

//==============================================================================

esp_err_t HttpServer::SetPort(uint16_t port) {
  LockGuard lg(*this);
  if (this->port == port)
    return ESP_OK;
  this->port = port;
  ESP_RETURN_ON_ERROR(RestartIfEnabled(), TAG, "restart failed");
  return ESP_OK;
}

//==============================================================================

size_t HttpServer::GetMaxNumberOfClients() {
  LockGuard lg(*this);
  return maxNumberOfClients;
}

//==============================================================================

esp_err_t HttpServer::SetMaxNumberOfClients(size_t maxNumberOfClients) {
  LockGuard lg(*this);
  if (this->maxNumberOfClients == maxNumberOfClients)
    return ESP_OK;
  this->maxNumberOfClients = maxNumberOfClients;
  ESP_RETURN_ON_ERROR(RestartIfEnabled(), TAG, "restart failed");
  return ESP_OK;
}
//==============================================================================

TickType_t HttpServer::GetReadTimeout() {
  LockGuard lg(*this);
  return readTimeout;
}

//==============================================================================

esp_err_t HttpServer::SetReadTimeout(TickType_t timeout) {
  LockGuard lg(*this);
  if (this->readTimeout == timeout)
    return ESP_OK;
  this->readTimeout = timeout;
  ESP_RETURN_ON_ERROR(RestartIfEnabled(), TAG, "restart failed");
  return ESP_OK;
}

//==============================================================================

TickType_t HttpServer::GetWriteTimeout() {
  LockGuard lg(*this);
  return writeTimeout;
}

//==============================================================================

esp_err_t HttpServer::SetWriteTimeout(TickType_t timeout) {
  LockGuard lg(*this);
  if (this->writeTimeout == timeout)
    return ESP_OK;
  this->writeTimeout = timeout;
  ESP_RETURN_ON_ERROR(RestartIfEnabled(), TAG, "restart failed");
  return ESP_OK;
}

//==============================================================================

esp_err_t HttpServer::SetTaskParameters(const TaskParameters& taskParameters) {
  LockGuard lg(*this);
  if (this->taskParameters == taskParameters)
    return ESP_OK;
  this->taskParameters = taskParameters;
  ESP_RETURN_ON_ERROR(RestartIfEnabled(), TAG, "restart failed");
  return ESP_OK;
}

//==============================================================================

esp_err_t HttpServer::HandleRequest(httpd_req_t* req) {
  HttpServer& server = *(HttpServer*)req->user_ctx;
  auto headerBuffer = server.headerBuffer;

  LockGuard lgServer(server, *headerBuffer);
  Transaction transaction(server, req);
  server.headerDataEnd = (char*)headerBuffer->data;

  server.handlingRequest = true;
  server.requestEvent.Generate(transaction);
  esp_err_t err = server.HandleRequest(transaction);
  server.handlingRequest = false;
  if (err != ESP_OK && !transaction.IsResponseWritten())
    transaction.WriteResponse(500);

  if (server.disableFromRequest &&
      xTaskCreatePinnedToCore(RestartTaskCode, "pl_http_server_restart", 4096, &server, server.taskParameters.priority, NULL, server.taskParameters.coreId) != pdPASS)
    ESP_LOGE(TAG, "restart task create failed");

  ESP_RETURN_ON_ERROR(err, TAG, "handle request failed");
  return ESP_OK;
}

//==============================================================================

void HttpServer::RestartTaskCode(void* parameters) {
  HttpServer& server = *(HttpServer*)parameters;
  {
    LockGuard lg(server);
    server.disableFromRequest = false;
    server.Disable();
    if (server.enableFromRequest) {
      server.enableFromRequest = false;
      server.Enable();
    }
  }
  vTaskDelete(NULL);
}

//==============================================================================

esp_err_t HttpServer::RestartIfEnabled() {
  if (!enabled || disableFromRequest)
    return ESP_OK;
  ESP_RETURN_ON_ERROR(Disable(), TAG, "disable failed");
  ESP_RETURN_ON_ERROR(Enable(), TAG, "enable failed");
  return ESP_OK;
}

//==============================================================================

HttpServer::Transaction::Transaction(HttpServer& server, httpd_req_t* req) :
  server(server), req(req), networkStream(std::make_shared<NetworkStream>(httpd_req_to_sockfd(req))) {
  networkStream->SetReadTimeout(server.readTimeout);
  networkStream->SetWriteTimeout(server.writeTimeout);
}

//==============================================================================

esp_err_t HttpServer::Transaction::ReadRequestBody(void* dest, size_t size) {
  ESP_RETURN_ON_FALSE(!responseWritten, ESP_ERR_INVALID_STATE, TAG, "response has already been sent");
  if (!size)
    return ESP_OK;

  TimeOut_t xTimeOut;
  vTaskSetTimeOutState(&xTimeOut);
  TickType_t remainingTimeout = server.readTimeout;

  int res = 0;
  do {
    if (dest) {
      res = httpd_req_recv(req, (char*)dest, size);
      if (res > 0) {
        size -= res;
        dest = (uint8_t*)dest + res;
      }
    }
    else {
      constexpr size_t discardBufferSize = 64;
      char discardBuffer[discardBufferSize];
      res = httpd_req_recv(req, discardBuffer, std::min(size, discardBufferSize));
      if (res > 0)
        size -= res;
    }
  } while (size && res > 0 && xTaskCheckForTimeOut(&xTimeOut, &remainingTimeout) == pdFALSE);

  if (!size)
    return ESP_OK;

  if (res > 0 || res == HTTPD_SOCK_ERR_TIMEOUT) {
    responseWritten = true;
    httpd_resp_send_408(req);
    ESP_RETURN_ON_ERROR(ESP_ERR_TIMEOUT, TAG, "timeout");
  }
  ESP_RETURN_ON_ERROR(ESP_FAIL, TAG, "request receive failed");
  return ESP_OK;
}

//==============================================================================

esp_err_t HttpServer::Transaction::WriteResponse(uint16_t statusCode, const void* body, size_t bodySize) {
  ESP_RETURN_ON_FALSE(!responseWritten, ESP_ERR_INVALID_STATE, TAG, "response has already been sent");

  std::string status = std::to_string(statusCode) + " ";
  auto statusCodeIterator = httpStatusCodeMap.find(statusCode);
  if (statusCodeIterator != httpStatusCodeMap.end())
    status += statusCodeIterator->second;
  ESP_RETURN_ON_ERROR(httpd_resp_set_status(req, status.c_str()), TAG, "set status failed");
  responseWritten = true;
  ESP_RETURN_ON_ERROR(httpd_resp_send(req, (char*)body, bodySize), TAG, "response send failed");
  return ESP_OK;
}

//==============================================================================

std::shared_ptr<NetworkStream> HttpServer::Transaction::GetNetworkStream() {
  return networkStream;
}

//==============================================================================

HttpMethod HttpServer::Transaction::GetRequestMethod() {
  auto method = httpMethodMap.find(req->method);
  return method != httpMethodMap.end() ? method->second : HttpMethod::unknown;
}

//==============================================================================

esp_err_t HttpServer::Transaction::GetRequestUri(std::string& uri) {
  ESP_RETURN_ON_FALSE(!responseWritten, ESP_ERR_INVALID_STATE, TAG, "response has already been sent");

  uri = req->uri;
  return ESP_OK;
}

//==============================================================================

esp_err_t HttpServer::Transaction::GetRequestHeader(const std::string& name, std::string& value) {
  ESP_RETURN_ON_FALSE(!responseWritten, ESP_ERR_INVALID_STATE, TAG, "response has already been sent");
  
  size_t valueSize = httpd_req_get_hdr_value_len(req, name.c_str()) + 1;
  std::vector<char> tempValue(valueSize);
  esp_err_t error = httpd_req_get_hdr_value_str(req, name.c_str(), tempValue.data(), valueSize);
  if (error == ESP_ERR_NOT_FOUND)
    ESP_LOGD(TAG, "header not found");
  else if (error != ESP_OK)
    ESP_LOGE(TAG, "get header value string failed");
  else
    value = tempValue.data();
  return error;
}

//==============================================================================

size_t HttpServer::Transaction::GetRequestBodySize() {
  return req->content_len;
}

//==============================================================================

esp_err_t HttpServer::Transaction::SetResponseHeader(const std::string& name, const std::string& value) {
  ESP_RETURN_ON_FALSE(!responseWritten, ESP_ERR_INVALID_STATE, TAG, "response has already been sent");

  char*& headerDataEnd = server.headerDataEnd;
  ESP_RETURN_ON_FALSE(headerDataEnd - (char*)server.headerBuffer->data + name.size() + value.size() + 2 <= server.headerBuffer->size, \
                      ESP_ERR_INVALID_SIZE, TAG, "header buffer is too small");
  char* nameStr = headerDataEnd;
  memcpy(headerDataEnd, name.c_str(), name.size() + 1);
  headerDataEnd += name.size() + 1;
  char* valueStr = headerDataEnd;
  memcpy(headerDataEnd, value.c_str(), value.size() + 1);
  headerDataEnd += value.size() + 1;

  ESP_RETURN_ON_ERROR(httpd_resp_set_hdr(req, nameStr, valueStr), TAG, "set header failed");
  return ESP_OK;
}

//==============================================================================

bool HttpServer::Transaction::IsResponseWritten() {
  return responseWritten;
}

//==============================================================================

}