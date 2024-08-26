#include "http_client.h"
#include "esp_crt_bundle.h"
#include "unity.h"

//==============================================================================

const TickType_t readTimeout = 5000 / portTICK_PERIOD_MS;
const std::string hostname = "httpbin.org";

const std::vector<TestTransaction> testTransactions= {
  {"GET", PL::HttpAuthScheme::none, "", "", PL::HttpMethod::GET, "/get", {}, "", 200, {}, {}},
  {"POST", PL::HttpAuthScheme::none, "", "", PL::HttpMethod::POST, "/post", {}, "Test data", 200, {}, {"\"data\": \"Test data\""}},
  {"PUT", PL::HttpAuthScheme::none, "", "", PL::HttpMethod::PUT, "/put", {}, "", 200, {}, {}},
  {"PATCH", PL::HttpAuthScheme::none, "", "", PL::HttpMethod::PATCH, "/patch", {}, "", 200, {}, {}},
  {"DELETE", PL::HttpAuthScheme::none, "", "", PL::HttpMethod::DELETE, "/delete", {}, "", 200, {}, {}},

  {"Basic Auth", PL::HttpAuthScheme::basic, "user", "password", PL::HttpMethod::GET, "/basic-auth/user/password", {}, "", 200, {}, {}},
  {"Digest Auth (auth)", PL::HttpAuthScheme::digest, "user", "password", PL::HttpMethod::GET, "/digest-auth/auth/user/password", {}, "", 200, {}, {}},
  {"Digest Auth (auth-int)", PL::HttpAuthScheme::digest, "user", "password", PL::HttpMethod::GET, "/digest-auth/auth-int/user/password", {}, "", 200, {}, {}},
  {"Digest Auth (auth, MD5)", PL::HttpAuthScheme::digest, "user", "password", PL::HttpMethod::GET, "/digest-auth/auth/user/password/MD5", {}, "", 200, {}, {}},
  {"Digest Auth (auth-int, MD5)", PL::HttpAuthScheme::digest, "user", "password", PL::HttpMethod::GET, "/digest-auth/auth-int/user/password/MD5", {}, "", 200, {}, {}},
  {"Digest Auth (auth, SHA-256)", PL::HttpAuthScheme::digest, "user", "password", PL::HttpMethod::GET, "/digest-auth/auth/user/password/SHA-256", {}, "", 200, {}, {}},
  {"Digest Auth (auth-int, SHA-256)", PL::HttpAuthScheme::digest, "user", "password", PL::HttpMethod::GET, "/digest-auth/auth-int/user/password/SHA-256", {}, "", 200, {}, {}},
  {"Digest Auth (auth, SHA-512)", PL::HttpAuthScheme::digest, "user", "password", PL::HttpMethod::GET, "/digest-auth/auth/user/password/SHA-512", {}, "", 200, {}, {}},
  {"Digest Auth (auth-int, SHA-512)", PL::HttpAuthScheme::digest, "user", "password", PL::HttpMethod::GET, "/digest-auth/auth-int/user/password/SHA-512", {}, "", 200, {}, {}},

  {"Status code", PL::HttpAuthScheme::none, "", "", PL::HttpMethod::GET, "/status/418", {}, "", 418, {}, {}},

  {"Request headers", PL::HttpAuthScheme::none, "", "", PL::HttpMethod::GET, "/headers", { {"A", "B"}, {"C", "D"} }, "", 200, {}, {"\"A\": \"B\"", "\"C\": \"D\""} },
  {"Response headers", PL::HttpAuthScheme::none, "", "", PL::HttpMethod::GET, "/response-headers?A=B&C=D", {}, "", 200, { {"A", "B"}, {"C", "D"} }, {}}
};

static char responseBody[2000];

//==============================================================================

void TestClient(PL::HttpClient& client) {
  TEST_ASSERT(client.Initialize() == ESP_OK);

  TEST_ASSERT_EQUAL(PL::HttpClient::defaultReadTimeout, client.GetReadTimeout());
  TEST_ASSERT(client.SetReadTimeout(readTimeout) == ESP_OK);
  TEST_ASSERT_EQUAL(readTimeout, client.GetReadTimeout());

  ushort responseStatusCode;
  size_t responseBodySize;
  for (auto& t : testTransactions) {
    printf("Test %s\n", t.name.c_str());
    TEST_ASSERT(client.SetAuthScheme(t.authScheme) == ESP_OK);
    TEST_ASSERT(client.SetAuthCredentials(t.username, t.password) == ESP_OK);

    for (auto& h : t.requestHeaders)
      TEST_ASSERT(client.SetRequestHeader(h.first, h.second) == ESP_OK);

    TEST_ASSERT(client.WriteRequest(t.requestMethod, t.requestUri, t.requestBody) == ESP_OK);
    TEST_ASSERT(client.ReadResponseHeaders(responseStatusCode, &responseBodySize) == ESP_OK); 
    if (t.authScheme == PL::HttpAuthScheme::digest && responseStatusCode == 401) {
      TEST_ASSERT(client.WriteRequest(t.requestMethod, t.requestUri, t.requestBody) == ESP_OK);
      TEST_ASSERT(client.ReadResponseHeaders(responseStatusCode, &responseBodySize) == ESP_OK);
    }
    TEST_ASSERT_EQUAL(t.responseStatusCode, responseStatusCode);
  
    for (auto& h : t.responseHeaders) {
      TEST_ASSERT(client.GetResponseHeader(h.first) != NULL);
      TEST_ASSERT(client.GetResponseHeader(h.first) == h.second);
    }      

    TEST_ASSERT(responseBodySize + 1 <= sizeof(responseBody));
    TEST_ASSERT(client.ReadResponseBody(responseBody, responseBodySize) == ESP_OK);
    responseBody[responseBodySize] = 0;
    
    for (auto& s : t.responseBodyStrings)
      TEST_ASSERT(strstr(responseBody, s.c_str()) != NULL);
  }

  printf("Test delay\n");
  TEST_ASSERT(client.WriteRequest(PL::HttpMethod::GET, "/delay/1") == ESP_OK);
  TEST_ASSERT(client.ReadResponseHeaders(responseStatusCode, NULL) == ESP_OK);
  TEST_ASSERT(client.WriteRequest(PL::HttpMethod::GET, "/delay/5") == ESP_OK);
  TEST_ASSERT(client.ReadResponseHeaders(responseStatusCode, NULL) == ESP_FAIL);

  TEST_ASSERT(client.Disconnect() == ESP_OK);
}

//==============================================================================

void TestHttpClient() {
  PL::HttpClient client(hostname);
  TEST_ASSERT_EQUAL(PL::HttpClient::defaultHttpPort, client.GetPort());
  TestClient(client);
}

//==============================================================================

void TestHttpsClient() {
  PL::HttpClient client(hostname, esp_crt_bundle_attach);
  TEST_ASSERT_EQUAL(PL::HttpClient::defaultHttpsPort, client.GetPort());
  TestClient(client);
}