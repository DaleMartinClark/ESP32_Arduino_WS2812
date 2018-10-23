#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include "esp32_digital_led_lib.h"

WiFiMulti wifiMulti;
strand_t pStrand = {.rmtChannel = 0, .gpioNum = 13, .ledType = LED_WS2812B_V2, .brightLimit = 32, .numPixels = 144, .pixels = nullptr, ._stateVars = nullptr};

uint8_t dataHead[54];
uint8_t dataStrip[432];
char buff[4096] = {0};
bool bRet = false;
int rowNum = 0;
int rowIndex = 0;

void httpGetImages()
{
    if ((wifiMulti.run() == WL_CONNECTED))
    {
        HTTPClient http;
        Serial.print("[HTTP] begin...\n");
        http.begin("https://www.dmcart.cn/weixin/weixin_images.bmp");
        Serial.print("[HTTP] GET...\n");
        int httpCode = http.GET();
        if (httpCode > 0)
        {
            Serial.printf("[HTTP] GET... code: %d\n", httpCode);
            if (httpCode == HTTP_CODE_OK)
            {
                int len = http.getSize();
                Serial.printf("[HTTP] GET... datasize: %d\n", len);
                for (int i = 0; i < len / 4096 + 1; i++)
                {
                    bRet = ESP.flashEraseSector(768 + i);
                    if (!bRet)
                        break;
                }
                if (bRet)
                    Serial.println("Flash erase OK");
                else
                    Serial.println("Flash erase failed");
                WiFiClient *stream = http.getStreamPtr();
                int total = 0;
                while (http.connected() && (len > 0 || len == -1))
                {
                    size_t size = stream->available();
                    if (size)
                    {
                        int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
                        bRet = ESP.flashWrite(0x300000 + total, (uint32_t *)buff, c);
                        if (bRet)
                            Serial.println("Flash write OK");
                        else
                            Serial.println("Flash write failed");

                        total += c;
                        if (len > 0)
                        {
                            len -= c;
                        }
                    }
                    delay(1);
                }
                Serial.print("Total size:");
                Serial.println(total);
                Serial.print("[HTTP] connection closed or file end.\n");
            }
        }
        else
        {
            Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
        }
        http.end();
    }
}
void initReadBitmap()
{
    bRet = ESP.flashRead(0x300000, (uint32_t *)dataHead, 54);
    rowNum = dataHead[22] + 256 * dataHead[23];
    if (bRet)
        Serial.println("Flash read OK");
    else
        Serial.println("Flash read failed");
    Serial.println(rowNum);
}
void startLedTransfer()
{
    uint32_t currentPos = 0;
    for (int i = 0; i < rowNum; i++)
    {
        currentPos = 432 * i + 54;
        bRet = ESP.flashRead(0x300000 + currentPos, (uint32_t *)dataStrip, 432);
        pStrand.pixels = dataStrip;
        digitalLeds_updatePixels(&pStrand);
        delay(1);
    }
}
void setup()
{
    Serial.begin(115200);
    wifiMulti.addAP("B507-Lab", "B5074321");
    httpGetImages();
    if (digitalLeds_initStrands(&pStrand, 1))
    {
        Serial.println("Init FAILURE: halting");
        while (true)
        {
        };
    }
    initReadBitmap();
    startLedTransfer();
}
void loop()
{
    startLedTransfer();
}