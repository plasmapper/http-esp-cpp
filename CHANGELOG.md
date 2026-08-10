# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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