#pragma once
#include <Arduino.h>
#include <WiFi.h>
// #include <Wire.h>

// I2C running on custom pins
// #define RTC_SDA 17
// #define RTC_CLK 16

// DS3231 RTC object
// extern ErriezDS3231 rtc;

// Try and get a static IP address
IPAddress local_IP(192, 168, 1, 112);
IPAddress gateway(192, 168, 1, 254);
IPAddress subnet(255, 255, 255, 0);

// To get the local time we must include DNS servers
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);

namespace WIFI {
const char* wl_status_to_string(wl_status_t status)
{
    switch (status) {
    case WL_NO_SHIELD:
        return "WL_NO_SHIELD";
    case WL_IDLE_STATUS:
        return "WL_IDLE_STATUS";
    case WL_NO_SSID_AVAIL:
        return "WL_NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED:
        return "WL_SCAN_COMPLETED";
    case WL_CONNECTED:
        return "WL_CONNECTED";
    case WL_CONNECT_FAILED:
        return "WL_CONNECT_FAILED";
    case WL_CONNECTION_LOST:
        return "WL_CONNECTION_LOST";
    case WL_DISCONNECTED:
        return "WL_DISCONNECTED";
    default:
        return "UNKNOWN";
    }
}

// Configure the local network connection with specified IP address
bool getTimeFromInternet()
{
    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false);
    WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);
    WiFi.begin("Greypuss", "mrSeal89");

    log_d("Connecting to WiFi (60 second timeout)");
    uint8_t wifiStatus = WiFi.waitForConnectResult();

    // Successful connection?
    if ((wl_status_t)wifiStatus != WL_CONNECTED) {
        log_e("WiFi Status: %s, exiting\n", wl_status_to_string((wl_status_t)wifiStatus));
        return false;
    }

    // Just some information about the Wifi channel & IP address
    log_i(
        "Running on SSID %s with IP %s, Channel %d, MAC address %sF",
        (char*)WiFi.SSID().c_str(),
        (char*)(WiFi.localIP()).toString().c_str(),
        WiFi.channel(),
        (char*)WiFi.macAddress().c_str());

    configTime(0, 0, "europe.pool.ntp.org");
    setenv("TZ", "GMT0BST,M3.5.0/01,M10.5.0/02", 1);

    log_i("Getting time from NTP server");
    while (!getLocalTime(&timeinfo)) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    Serial.println(&timeinfo, "%d-%b-%y, %H:%M:%S");

    // disconnect WiFi as it's no longer needed
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

    // Update the DS3231 with new time
    if (!rtc.setTime(timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec)) {
        log_w("Could not set the RTC module's time");
        return false;
    } else {
        log_i("Successfully (re-)set the RTC module's time");
    }

    if (!rtc.read(&timeinfo)) {
        log_w("Cannot read time from RTC DS3231 module");
    }

    // All done
    return true;
}

// Initialise the DS3231 RTC module on the I2C bus
void rtcBegin()
{
    // I2C is on: SDA & SCL=4, Speed=100K
    Wire.begin(RTC_SDA, RTC_CLK);
    while (!rtc.begin()) {
        log_w("Searching for RTC module on pins SDA=16 & SCL=4");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

// Stop TwoWire
void rtcEnd()
{
    Wire.end();
}

} // namespace WIFI