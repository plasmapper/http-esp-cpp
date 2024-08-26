#pragma once
#include "pl_common.h"
#include "pl_network.h"
#include "pl_http_types.h"

//==============================================================================

namespace PL {

//==============================================================================

/// @brief HTTP/HTTPS server transaction class
class HttpServerTransaction {
public:
  /// @brief Gets the transaction network stream
  /// @return network stream
  virtual std::shared_ptr<NetworkStream> GetNetworkStream() = 0;
  
  /// @brief Reads the request body
  /// @param dest destination (can be NULL)
  /// @param size number of bytes to read
  /// @return error code
  virtual esp_err_t ReadRequestBody(void* dest, size_t size) = 0;

  /// @brief Writes the response
  /// @param statusCode status code
  /// @param body body
  /// @param bodySize body size
  /// @return error code
  virtual esp_err_t WriteResponse(uint16_t statusCode, const void* body, size_t bodySize) = 0;

  /// @brief Writes the response
  /// @param statusCode status code
  /// @param body body
  /// @return error code
  esp_err_t WriteResponse(uint16_t statusCode, const std::string& body);

  /// @brief Writes the response with the status code 200
  /// @param body body
  /// @param bodySize body size
  /// @return error code
  esp_err_t WriteResponse(const void* body, size_t bodySize);

  /// @brief Writes the response with the status code 200
  /// @param body body
  /// @return error code
  esp_err_t WriteResponse(const std::string& body);

  /// @brief Writes the response with no body
  /// @param statusCode status code
  /// @return error code
  esp_err_t WriteResponse(uint16_t statusCode);

  /// @brief Gets the request HTTP method
  /// @return HTTP method
  virtual HttpMethod GetRequestMethod() = 0;

  /// @brief Gets the request URI
  /// @return URI
  virtual const char* GetRequestUri() = 0;

  /// @brief Gets the request header value
  /// @param name header name
  /// @return header value (NULL if no such header in the request)
  virtual const char* GetRequestHeader(const std::string& name) = 0;

  /// @brief Gets the request body size
  /// @return body size
  virtual size_t GetRequestBodySize() = 0;

  /// @brief Sets the response header
  /// @param name header name
  /// @param value header value
  /// @return error code
  virtual esp_err_t SetResponseHeader(const std::string& name, const std::string& value) = 0;
};

//==============================================================================

}