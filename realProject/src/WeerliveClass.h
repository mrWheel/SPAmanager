/*
***************************************************************************
**  File : WeerliveClass.h
**  Ported for ESP32 by Willem Aandewiel - 2025
**
**  Original Copyright (c) 2024 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
*/

#ifndef WEERLIVE_H
#define WEERLIVE_H

#include <WiFi.h>          
#include <WiFiClient.h>
#include <ArduinoJson.h>   

class Weerlive 
{
public:
    Weerlive(WiFiClient &weerliveClient);
    void setup(const char *key, const char *city, Stream* debugPort);
    void setInterval(int newInterval);
    const char *request();
    bool loop(std::string& result);
    
private:
    WiFiClient         &thisClient;
    String              apiUrl;
    static const char  *apiHost;
    String              apiKey;
    String              weerliveText;
    char                jsonResponse[10000];   
    int                 alarmInd = -1;
    uint32_t            requestInterval = 0;
    uint32_t            loopTimer = 0;
    const char*         dateToDayName(const char* date_str);
    Stream* debug = nullptr; // Optional, default to nullptr


    StaticJsonDocument<10000> doc;
    StaticJsonDocument<1000> filter;

    void configureFilters();
  #ifdef WEERLIVE_DEBUG
    bool doDebug = true;
  #else
    bool doDebug = false;
  #endif

};

#endif // WEERLIVE_H

/***************************************************************************
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to permit
* persons to whom the Software is furnished to do so, subject to the
* following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
* THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
****************************************************************************
*/