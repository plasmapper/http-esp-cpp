# HTTP/HTTPS Client Example

1. Internal ESP Wi-Fi station interface is created and initialized.
2. The Wi-Fi station SSID and password are set from project configuration (can be changed in `Example` menu of [Project Configuration](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/kconfig.html)).
3. Two clients (for http://example.com and https://example.com) are created and initialized.
4. The Wi-Fi station is enabled.
5. After the interface gets an IP address both clients execute GET requests and the results are printed.