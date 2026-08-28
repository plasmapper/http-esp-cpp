# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]
### Fixed
- HttpServer::defaultHttpName and defaultHttpsName static initialization order dependency.
- HttpServer::StopTask deadlocking against an in-flight HandleRequest call by holding the server lock across httpd_ssl_stop.

## [2.2.2] - 2026-08-27
### Fixed
- HttpServer::SetPort, SetMaxNumberOfClients, SetReadTimeout, SetWriteTimeout and SetTaskParameters restarting the server even when the value did not change.

## [2.2.1] - 2026-08-25
### Fixed
- HttpServer destruction sequence.

## [2.2.0] - 2026-08-24
### Added
- Virtual destructor to HttpServerTransaction.

### Changed
- HttpServerTransaction to be non-copyable.

### Fixed
- HttpServer task not being stopped before the derived object is destroyed.
- HttpServer server handle left dangling after the server is stopped.
- HttpServer deadlock when Enable, Disable or a configuration setter is called from HandleRequest.
- Transaction network stream overriding the server's read and write timeouts with NetworkStream's defaults.
- HttpClient::GetResponseHeader missing a trailing header with an empty value.
- Missing documentation for HttpServer::Enable claiming the server's UDP port for its control socket.
- HttpClient::ReadResponseHeaders using the non-standard ushort instead of uint16_t.
- HttpServer::StopTask returning httpd_unregister_uri error.
- Transaction::GetRequestHeader logging a missing header as an error.
- HttpClient::SetPort leaving clientHandle dangling if esp_http_client_cleanup fails.

## [2.1.1] - 2026-08-20
### Fixed
- HttpServer::Transaction::ReadRequestBody not enforcing an overall timeout.
- HttpServer::Enable rollback.

## [2.1.0] - 2026-08-14
### Added
- Write operation timeout to HttpClient and HttpServer.

### Fixed
- Read timeout conversion to seconds.
- Discard read performance when discarding data one byte at a time.
- Null certificate/private key handling.

## [2.0.0] - 2026-08-10
### Changed
- Lock timeout handling.
- Static const members to constexpr.
- HttpServer::GetRequestUri, HttpServer::GetRequestHeader and HttpClient::GetResponseHeader to return esp_err_t with an output parameter instead of a raw pointer.
- HttpServer::GetRequestHeader to use the ESP-IDF header API instead of reading httpd_req_t internal struct layout.

### Removed
- HttpServer separate URI buffer.
- HttpClient constructors taking a shared header buffer.

### Fixed
- HttpServer::Transaction::ReadRequestBody not looping on partial reads.
- HttpClient::Disconnect missing lock and initialization check.
- HttpClient::GetResponseHeader missing shared header buffer lock.
- HttpServer::maxNumberOfClients type mismatch with NetworkServer interface.
- HttpServer::Disable leaving the server half-stopped when unregistering the URI handler fails.
- HttpClient::SetPort client cleanup.
- HttpClient::GetResponseHeader out-of-bounds pointer arithmetic.

## [1.1.0] - 2024-08-26
### Changed
- Version minor number to indicate changed ESP-IDF dependency.

## [1.0.2] - 2024-08-26
### Changed
- Client and server transaction locking.

## [1.0.1] - 2024-06-12
### Added
- Copying examples to component folder on upload.

## [1.0.0] - 2024-06-12
Initial release.