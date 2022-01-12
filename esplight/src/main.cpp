// todo https://stackoverflow.com/questions/2548075/c-string-template-library
#include "Arduino.h"
#include "DNSServer.h"
#include "WebServer.h"
#include "WiFi.h"
#include "WiFiManager.h"
#include "WiFiUdp.h"
#include "date_struct.h"
#include "esp_adc_cal.h"
#include "light_helper.h"
#include "ntp_helper.h"
#include "storage_helper.h"
#include "time.h"

#define EEPROM_SIZE 1

void espDelay(int ms) {
    esp_sleep_enable_timer_wakeup(ms * 1000);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    esp_light_sleep_start();
}

WiFiManager wifiManager;

LighTimeStorage lightStorage = LighTimeStorage();
WebServer server(80);
LightHelper light = LightHelper();
NTPHelper ntpHelper = NTPHelper();

void saveConfigCallback() {
    Serial.println("Configuração salva");
}

void connectToWifi() {
    String portalName = "ESPLIGHT-WIFI";

    WiFi.setHostname(portalName.c_str());
    wifiManager.setSTAStaticIPConfig(IPAddress(192, 168, 1, 99), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));

    if (!wifiManager.autoConnect(portalName.c_str())) {
        espDelay(3000);
        //reset and try again, or maybe put it to deep sleep
        ESP.restart();
        espDelay(5000);
    }
    espDelay(3000);
}

void setCrossOrigin() {
    server.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
    server.sendHeader(F("Access-Control-Max-Age"), F("600"));
    server.sendHeader(F("Access-Control-Allow-Methods"), F("PUT,POST,GET,OPTIONS"));
    server.sendHeader(F("Access-Control-Allow-Headers"), F("*"));
};

void handleNotFound() {
    if (server.method() == HTTP_OPTIONS) {
        setCrossOrigin();
        server.send(204);
    } else {
        server.send(404, "text/plain", "");
    }
}

void sendCrossOriginHeader() {
    Serial.println(F("sendCORSHeader"));
    setCrossOrigin();
    server.send(204);
}

void handleToggle() {
    light.setForceLight(!light.forceLight);
    setCrossOrigin();
    server.send(200, "text/plain", "toggled light");
}

void handleVerify() {
    // String resText = "light is ";
    // resText += light.forceLight ? "on" : "off";
    String resText = "{\"force\":";
    resText += (light.forceLight ? "true" : "false");
    resText += "}";
    setCrossOrigin();
    server.send(200, "application/json", resText);
}

void handleLightTimes() {
    setCrossOrigin();
    server.send(200, "application/json", lightStorage.getTimesAsJson());
}

void handleUpdateLightTimes() {
    lightStorage.save(server.arg(0));
    setCrossOrigin();
    server.send(200, "application/json", lightStorage.getTimesAsJson());
}

void setup() {
    Serial.begin(9600);
    Serial.println("Start");
    lightStorage.setup();

    wifiManager.setConfigPortalTimeout(180);

    lightStorage.load();

    connectToWifi();

    server.on("/", HTTP_GET,
              []() { server.send(200, "text/plain", "Welcome to reeflight :)"); });

    server.on("/toggle", HTTP_GET,
              handleToggle);

    server.on("/verify", HTTP_GET,
              handleVerify);

    server.on("/light-times", HTTP_GET,
              handleLightTimes);

    server.on("/light-times", HTTP_OPTIONS, sendCrossOriginHeader);

    server.on("/light-times", HTTP_PUT,
              handleUpdateLightTimes);

    server.onNotFound(handleNotFound);

    server.begin();
}

void loop() {
    Date timeNow = ntpHelper.getTime();

    light.loop(timeNow, lightStorage.timesSaved, lightStorage.lightTimes);

    server.handleClient();
}
