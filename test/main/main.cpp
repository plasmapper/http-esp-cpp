#include "unity.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "http_client.h"
#include "http_server.h"

//==============================================================================

PL::EspWiFiStation wifi;
const std::string wifiSsid = CONFIG_TEST_WIFI_SSID;
const std::string wifiPassword = CONFIG_TEST_WIFI_PASSWORD;

//==============================================================================

extern "C" void app_main(void) {
  ESP_ERROR_CHECK (esp_event_loop_create_default());
  ESP_ERROR_CHECK (esp_netif_init());

  ESP_ERROR_CHECK (wifi.Initialize());
  ESP_ERROR_CHECK (wifi.SetSsid (wifiSsid));
  ESP_ERROR_CHECK (wifi.SetPassword (wifiPassword));
  ESP_ERROR_CHECK (wifi.Enable());

  while (!wifi.GetIpV4Address().u32)  
    vTaskDelay (1);

  UNITY_BEGIN();
  RUN_TEST (TestHttpClient);
  RUN_TEST (TestHttpsClient);
  RUN_TEST (TestHttpServer);
  RUN_TEST (TestHttpsServer);
  UNITY_END();
}