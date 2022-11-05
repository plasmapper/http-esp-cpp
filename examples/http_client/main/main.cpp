#include "pl_network.h"
#include "pl_http.h"
#include "esp_crt_bundle.h"

//==============================================================================

PL::EspWiFiStation wifi;
const std::string wifiSsid = CONFIG_EXAMPLE_WIFI_SSID;
const std::string wifiPassword = CONFIG_EXAMPLE_WIFI_PASSWORD;

auto headerBuffer = std::make_shared<PL::Buffer>(1024);
const std::string host = "example.com";
const std::string path = "/";

PL::HttpClient httpClient (host, headerBuffer);
PL::HttpClient httpsClient (host, esp_crt_bundle_attach, headerBuffer);

char responseBody[2000];

//==============================================================================

extern "C" void app_main(void) {
  esp_event_loop_create_default();
  esp_netif_init();

  wifi.Initialize();
  wifi.SetSsid (wifiSsid);
  wifi.SetPassword (wifiPassword);
  wifi.Enable();

  while (!wifi.GetIpV4Address().u32)  
    vTaskDelay (1);

  httpClient.Initialize();
  httpsClient.Initialize();

  ushort responseStatusCode;
  size_t responseBodySize;

  printf ("GET %s from http://%s\n", path.c_str(), host.c_str());
  if (httpClient.WriteRequestHeaders (PL::HttpMethod::GET, path, 0) == ESP_OK && httpClient.ReadResponseHeaders (responseStatusCode, &responseBodySize) == ESP_OK) {
    printf ("Status code: %d, body size: %d\n", responseStatusCode, responseBodySize);
    if (auto date = httpClient.GetResponseHeader ("Date"))
      printf ("Date: %s\n", date);
    if (responseBodySize + 1 <= sizeof (responseBody)) {
      httpClient.ReadResponseBody (responseBody, responseBodySize);
      responseBody[responseBodySize] = 0;
      printf ("%s\n\n", responseBody);
    }
  }

  printf ("GET %s from https://%s\n", path.c_str(), host.c_str());
  if (httpsClient.WriteRequestHeaders (PL::HttpMethod::GET, path, 0) == ESP_OK && httpsClient.ReadResponseHeaders (responseStatusCode, &responseBodySize) == ESP_OK) {
    printf ("Status code: %d, body size: %d\n", responseStatusCode, responseBodySize);
    if (auto date = httpClient.GetResponseHeader ("Date"))
      printf ("Date: %s\n", date);
    if (responseBodySize + 1 <= sizeof (responseBody)) {
      httpsClient.ReadResponseBody (responseBody, responseBodySize);
      responseBody[responseBodySize] = 0;
      printf ("%s\n\n", responseBody);
    }
  }
  
  while (1) {
    vTaskDelay (1);
  }
}