#pragma once

//==============================================================================

namespace PL {

//==============================================================================

/// @brief HTTP method
enum class HttpMethod {
  /// @brief unknown method
  unknown,
  /// @brief GET method
  GET,
  /// @brief POST method
  POST,
  /// @brief PUT method
  PUT,
  /// @brief PATCH method
  PATCH,
  /// @brief DELETE method
  DELETE
};

enum class HttpAuthScheme {
  /// @brief no authentication
  none,
  /// @brief basic authentication
  basic,
  /// @brief digest authentication
  digest
};

//==============================================================================

}