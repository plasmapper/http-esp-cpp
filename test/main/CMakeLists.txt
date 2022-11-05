cmake_minimum_required(VERSION 3.5)

idf_component_register(SRCS "main.cpp" "http_client.cpp" "http_server.cpp" INCLUDE_DIRS "." EMBED_TXTFILES "cert.pem" "key.pem")