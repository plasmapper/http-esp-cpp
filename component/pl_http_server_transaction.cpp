#include "pl_http_server_transaction.h"

//==============================================================================

namespace PL {

//==============================================================================

esp_err_t HttpServerTransaction::WriteResponse (uint16_t statusCode, const void* body, size_t bodySize) {
  return WriteResponse (statusCode, body, bodySize);
}

//==============================================================================

esp_err_t HttpServerTransaction::WriteResponse (uint16_t statusCode, const std::string& body) {
  return WriteResponse (statusCode, body.data(), body.size());
}

//==============================================================================

esp_err_t HttpServerTransaction::WriteResponse (const void* body, size_t bodySize) {
  return WriteResponse (200, body, bodySize);
}

//==============================================================================

esp_err_t HttpServerTransaction::WriteResponse (const std::string& body) {
  return WriteResponse (200, body.data(), body.size());
}

//==============================================================================

esp_err_t HttpServerTransaction::WriteResponse (uint16_t statusCode) {
  return WriteResponse (statusCode, NULL, 0);
}

//==============================================================================

}