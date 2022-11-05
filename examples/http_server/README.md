# HTTP/HTTPS Server Example

1. HttpServer class inherits PL::HttpServer class and overrides HandleRequest method to return URI as a body of the response.
2. Two servers are created: HTTP (port 80) and HTTPS (port 443).
3. Internal ESP Wi-Fi station interface is created and initialized.
4. The Wi-Fi station SSID and password are set from project configuration (can be changed in `Example` menu of [Project Configuration](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/kconfig.html)).
5. The HTTPS server credentials are set from the embedded text files and HTTPS functionality is enabled.
6. The Wi-Fi station is enabled.
7. The servers are enabled when the Wi-Fi station interface gets an IP address.
8. The server events are used to print the client request information.