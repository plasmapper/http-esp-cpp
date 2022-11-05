#include "pl_network.h"
#include "pl_http.h"

//==============================================================================

class HttpServer : public PL::HttpServer {
public:
  using PL::HttpServer::HttpServer;

protected:
  esp_err_t HandleRequest (PL::HttpServerTransaction& transaction) override;
};

//==============================================================================

class WiFiGotIpEventHandler {
public:
  void OnGotIpV4Address (PL::NetworkInterface& wifi);
  void OnGotIpV6Address (PL::NetworkInterface& wifi);
};

//==============================================================================

class RequestEventHandler {
public:
  void OnRequest (PL::HttpServer& server, PL::HttpServerTransaction& transaction);
};

//==============================================================================

PL::EspWiFiStation wifi;
const std::string wifiSsid = CONFIG_EXAMPLE_WIFI_SSID;
const std::string wifiPassword = CONFIG_EXAMPLE_WIFI_PASSWORD;
auto wiFiGotIpEventHandler = std::make_shared<WiFiGotIpEventHandler>();

auto uriBuffer = std::make_shared<PL::Buffer>(512);
auto headerBuffer = std::make_shared<PL::Buffer>(1024);

HttpServer httpServer (uriBuffer, headerBuffer);

extern const char certificate[] asm("_binary_cert_pem_start");
extern const char privateKey[] asm("_binary_key_pem_start");
HttpServer httpsServer (certificate, privateKey, uriBuffer, headerBuffer);

auto requestEventHandler = std::make_shared<RequestEventHandler>();

//==============================================================================

extern "C" void app_main(void) {
  esp_event_loop_create_default();
  esp_netif_init();

  wifi.Initialize();
  wifi.SetSsid (wifiSsid);
  wifi.SetPassword (wifiPassword);
  wifi.gotIpV4AddressEvent.AddHandler (wiFiGotIpEventHandler, &WiFiGotIpEventHandler::OnGotIpV4Address);
  wifi.gotIpV6AddressEvent.AddHandler (wiFiGotIpEventHandler, &WiFiGotIpEventHandler::OnGotIpV6Address);

  httpServer.requestEvent.AddHandler (requestEventHandler, &RequestEventHandler::OnRequest);
  httpsServer.requestEvent.AddHandler (requestEventHandler, &RequestEventHandler::OnRequest);

  wifi.Enable();

  while (1) {
    vTaskDelay (1);
  }
}

//==============================================================================

esp_err_t HttpServer::HandleRequest (PL::HttpServerTransaction& transaction) {
  switch (transaction.GetRequestMethod()) {
    case PL::HttpMethod::GET:
      return transaction.WriteResponse (transaction.GetRequestUri());
    default:
      return transaction.WriteResponse (405);
  }
}

//==============================================================================

void WiFiGotIpEventHandler::OnGotIpV4Address (PL::NetworkInterface& wifi) {
  if (httpServer.Enable() == ESP_OK)
    printf ("Listening (address: %s, port: %d)\n", wifi.GetIpV4Address().ToString().c_str(), httpServer.GetPort());
  if (httpsServer.Enable() == ESP_OK)
    printf ("Listening (address: %s, port: %d)\n", wifi.GetIpV4Address().ToString().c_str(), httpsServer.GetPort());
}

//==============================================================================

void WiFiGotIpEventHandler::OnGotIpV6Address (PL::NetworkInterface& wifi) {
  if (httpServer.Enable() == ESP_OK)
    printf ("Listening (address: %s, port: %d)\n", wifi.GetIpV6LinkLocalAddress().ToString().c_str(), httpServer.GetPort());
  if (httpsServer.Enable() == ESP_OK)
    printf ("Listening (address: %s, port: %d)\n", wifi.GetIpV6LinkLocalAddress().ToString().c_str(), httpsServer.GetPort());
}

//==============================================================================

void RequestEventHandler::OnRequest (PL::HttpServer& server, PL::HttpServerTransaction& transaction) {
  printf ("%s request from %s\n", &server == &httpServer ? "HTTP" : "HTTPS", transaction.GetNetworkStream()->GetRemoteEndpoint().address.ToString().c_str());
  printf ("URI: %s\n", transaction.GetRequestUri());
  if (auto host = transaction.GetRequestHeader ("Host"))
    printf ("Host: %s\n", host);
  printf ("\n");
}