#include "pl_http.h"
#include <map>

//==============================================================================

struct TestTransaction {
  std::string name;

  PL::HttpAuthScheme authScheme;
  std::string username;
  std::string password;

  PL::HttpMethod requestMethod;
  std::string requestUri;
  std::map<std::string, std::string> requestHeaders;
  std::string requestBody;

  ushort responseStatusCode;
  std::map<std::string, std::string> responseHeaders;
  std::vector<std::string> responseBodyStrings;
};

//==============================================================================

void TestHttpClient();
void TestHttpsClient();