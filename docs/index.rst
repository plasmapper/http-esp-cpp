HTTP/HTTPS Component
====================

.. |COMPONENT| replace:: http

.. |VERSION| replace:: 1.0.1

.. include:: ../../../installation.rst

.. include:: ../../../sdkconfig_https.rst

Features
--------

1. :cpp:class:`PL::HttpClient` - an HTTP/HTTPS client class. It is initialized with a hostname and (for an HTTPS client) server certificate or certificate bundle.
   :cpp:func:`PL::HttpClient::Initialize` initializes the client.
   A request is performed using :cpp:func:`PL::HttpClient::WriteRequestHeaders`, :cpp:func:`PL::HttpClient::WriteRequestBody`,
   :cpp:func:`PL::HttpClient::ReadResponseHeaders` and :cpp:func:`PL::HttpClient::ReadResponseBody`.
   :cpp:func:`PL::HttpClient::SetRequestAuthScheme` and :cpp:func:`PL::HttpClient::SetRequestAuthCredentials` configure the HTTP authentication.
   :cpp:func:`PL::HttpClient::SetRequestHeader` and :cpp:func:`PL::HttpClient::DeleteRequestHeader` configure the request headers.
2. :cpp:class:`PL::HttpServer` - a :cpp:class:`PL::NetworkServer` implementation for HTTP/HTTPS connections. The descendant class should override
   :cpp:func:`PL::HttpServer::HandleRequest` to handle the client request.
3. :cpp:class:`PL::HttpServerTransaction` - an HTTP/HTTPS server transaction class.
   :cpp:func:`PL::HttpServerTransaction::GetRequestMethod`, :cpp:func:`PL::HttpServerTransaction::GetRequestUri`, :cpp:func:`PL::HttpServerTransaction::GetRequestHeader`,
   :cpp:func:`PL::HttpServerTransaction::GetRequestBodySize` and :cpp:func:`PL::HttpServerTransaction::ReadRequestBody` should be used to analyze the request.
   :cpp:func:`PL::HttpServerTransaction::SetResponseHeader` and :cpp:func:`PL::HttpServerTransaction::WriteResponse` should be used to send the response.

Examples
--------
| `HTTP/HTTPS client <https://components.espressif.com/components/plasmapper/pl_http/versions/1.0.1/examples/http_client>`_
| `HTTP/HTTPS server <https://components.espressif.com/components/plasmapper/pl_http/versions/1.0.1/examples/http_server>`_
  
API reference
-------------

.. toctree::
  
  api/types      
  api/http_client
  api/http_server
  api/http_server_transaction