#include "http_server.h"
#include "esp_crt_bundle.h"
#include "unity.h"
#include <map>

//==============================================================================

ushort port = 500;
const TickType_t readTimeout = 3000 / portTICK_PERIOD_MS;
const TickType_t writeTimeout = 4000 / portTICK_PERIOD_MS;
const size_t maxNumberOfClients = 2;
const std::string host = "localhost";

extern const char certificate[] asm("_binary_cert_pem_start");
extern const char privateKey[] asm("_binary_key_pem_start");

const PL::HttpMethod correctRequestMethod = PL::HttpMethod::GET;
const PL::HttpMethod incorrectRequestMethod = PL::HttpMethod::POST;
const std::string correctRequestUri = "/correct";
const std::string incorrectRequestUri = "/incorrect";
const std::map<std::string, std::string> requestHeaders = { {"A", "B"}, {"C", "D"} };
const std::string requestBody = "Test body";
ushort responseStatusCode;
size_t responseBodySize;
static char responseBody[100];

//==============================================================================

void TestServer(PL::HttpServer& server, PL::HttpClient& client) {
  TEST_ASSERT(client.Initialize() == ESP_OK);

  TEST_ASSERT_EQUAL(PL::HttpServer::defaultReadTimeout, server.GetReadTimeout());
  TEST_ASSERT(server.SetReadTimeout(readTimeout) == ESP_OK);
  TEST_ASSERT_EQUAL(readTimeout, server.GetReadTimeout());
  
  TEST_ASSERT_EQUAL(PL::HttpClient::defaultWriteTimeout, client.GetWriteTimeout());
  TEST_ASSERT(client.SetWriteTimeout(writeTimeout) == ESP_OK);
  TEST_ASSERT_EQUAL(writeTimeout, client.GetWriteTimeout());

  for (int p = 0; p < 2; p++) {
    TEST_ASSERT(server.SetPort(port) == ESP_OK);
    TEST_ASSERT(client.SetPort(port) == ESP_OK);
    TEST_ASSERT_EQUAL(port, server.GetPort());
    TEST_ASSERT_EQUAL(port, client.GetPort());

    TEST_ASSERT(server.SetMaxNumberOfClients(maxNumberOfClients) == ESP_OK);
    TEST_ASSERT_EQUAL(maxNumberOfClients, server.GetMaxNumberOfClients());
    TEST_ASSERT(server.Enable() == ESP_OK);
    TEST_ASSERT(server.IsEnabled());

    for (auto& h : requestHeaders) {
      TEST_ASSERT(client.SetRequestHeader(h.first, h.second) == ESP_OK);
    }
    TEST_ASSERT(client.WriteRequest(correctRequestMethod, correctRequestUri, requestBody) == ESP_OK);
    TEST_ASSERT(client.ReadResponseHeaders(responseStatusCode, &responseBodySize) == ESP_OK);
    TEST_ASSERT_EQUAL(200, responseStatusCode);
    for (auto& h : requestHeaders) {
      std::string responseHeaderValue;
      TEST_ASSERT(client.GetResponseHeader(h.first, responseHeaderValue) == ESP_OK);
      TEST_ASSERT(h.second == responseHeaderValue);
    }
    TEST_ASSERT_EQUAL(requestBody.size(), responseBodySize);
    TEST_ASSERT(client.ReadResponseBody(responseBody, responseBodySize) == ESP_OK);
    responseBody[responseBodySize] = 0;
    TEST_ASSERT(requestBody == responseBody); 

    TEST_ASSERT(client.WriteRequest(incorrectRequestMethod, correctRequestUri) == ESP_OK);
    TEST_ASSERT(client.ReadResponseHeaders(responseStatusCode, NULL) == ESP_OK); 
    TEST_ASSERT_EQUAL(405, responseStatusCode);

    TEST_ASSERT(client.WriteRequest(correctRequestMethod, incorrectRequestUri) == ESP_OK);
    TEST_ASSERT(client.ReadResponseHeaders(responseStatusCode, NULL) == ESP_OK);
    TEST_ASSERT_EQUAL(404, responseStatusCode);

    port++;
  }

  TEST_ASSERT(server.Disable() == ESP_OK);
  TEST_ASSERT(!server.IsEnabled());
}

//==============================================================================

HttpServer::~HttpServer() {
  StopTask();
}

//==============================================================================

esp_err_t HttpServer::HandleRequest(PL::HttpServerTransaction& transaction) {
  std::string requestUri, requestHeaderValue;
  char requestBody[100];
  size_t requestBodySize = transaction.GetRequestBodySize();

  switch (transaction.GetRequestMethod()) {
    case PL::HttpMethod::GET:
      transaction.GetRequestUri(requestUri);
      if (requestUri == correctRequestUri) {
        for (auto& header : requestHeaders) {
          if (transaction.GetRequestHeader(header.first, requestHeaderValue) == ESP_OK)
            transaction.SetResponseHeader(header.first, requestHeaderValue);
        }
        if (requestBodySize <= sizeof(requestBody)) {
          transaction.ReadRequestBody(requestBody, requestBodySize);
          return transaction.WriteResponse(200, requestBody, requestBodySize);
        }
        return transaction.WriteResponse(413);
      }
      else
        return transaction.WriteResponse(404);
    default:
      return transaction.WriteResponse(405);
  }
}

//==============================================================================

void TestHttpServer() {
  HttpServer server;
  PL::HttpClient client(host);
  TEST_ASSERT_EQUAL(PL::HttpClient::defaultHttpPort, server.GetPort());
  TestServer(server, client);
}

//==============================================================================

void TestHttpsServer() {
  HttpServer server(certificate, privateKey);
  PL::HttpClient client(host, certificate);
  TEST_ASSERT_EQUAL(PL::HttpClient::defaultHttpsPort, server.GetPort());
  TestServer(server, client);
}