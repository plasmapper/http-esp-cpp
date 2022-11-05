cmake_minimum_required(VERSION 3.5)

idf_component_register(SRCS "pl_http_client.cpp" "pl_http_server_transaction.cpp" "pl_http_server.cpp" 
                       INCLUDE_DIRS "include" REQUIRES "esp_http_client" "esp_https_server")