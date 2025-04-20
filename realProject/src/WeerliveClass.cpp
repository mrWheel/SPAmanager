/*
***************************************************************************
**  Program : WeerliveClass.cpp
**  Ported for ESP32 by Willem Aandewiel - 2025
**
**  Original Copyright (c) 2024 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
*/

#include "WeerliveClass.h"

const char *Weerlive::apiHost = "weerlive.nl";

const char *dayNames[] = {"Zondag", "Maandag", "Dinsdag", "Woensdag", "Donderdag", "Vrijdag", "Zaterdag"};

Weerlive::Weerlive(WiFiClient &thisClient) : thisClient(thisClient), apiUrl("") {}

/**
 * Initializes the Weerlive class by setting up the API URL with the provided key and city,
 * then starts the serial communication and configures filters.
 *
 * @param key The key for the API
 * @param city The city for which weather data is requested
 *
 * @throws None
 */
void Weerlive::setup(const char *key, const char *city, Stream* debugPort)
{
  debug = debugPort;
  char tmp[100];
  apiKey = key;
  if (debug) debug->flush();
  if (debug) debug->printf("\nWeerlive::setup() with apiKey[%s]\n", apiKey.c_str());
  snprintf(tmp, sizeof(tmp), "/api/weerlive_api_v2.php?key=%s&locatie=%s", key, city);
  apiUrl = String(tmp);
  if (debug) debug->printf("weerlive::setup() apiUrl = [%s]\n", apiUrl.c_str());
  configureFilters();
  debug->printf("weerlive::setup() Done! Free Heap: [%d] bytes\n", ESP.getFreeHeap());

} //  setup()

void Weerlive::setInterval(int newInterval)
{
  requestInterval = newInterval;

} //  setInterval()

bool Weerlive::loop(std::string& result)
{
  if (millis() > loopTimer && apiKey.length() > 5)
  {
    loopTimer = millis() + (requestInterval * (60 * 1000)); // Interval in Minutes!
    result = request();
    if (debug) debug->printf("weerlive::loop() request->result[%s]\n", result.c_str());
    return true;
  }
  return false;

} //  loop()

/**
 * Configures the filters for the Weerlive class.
 *
 * Sets up the filters for the 'liveweer' and 'wk_verw' objects to include specific
 * weather data.
 * The filters are used to determine which weather data to retrieve from the API.
 *
 * @throws None
 */
void Weerlive::configureFilters()
{
  filter["liveweer"][0]["fout"]     = true;   //-- LEAVE THIS!!!
  filter["liveweer"][0]["plaats"]   = true;
  filter["liveweer"][0]["time"]     = false;
  filter["liveweer"][0]["temp"]     = true;   //-- actuele temperatuur in graden Celsius
  filter["liveweer"][0]["gtemp"]    = false;  //-- gevoeld temperatuur
  filter["liveweer"][0]["samenv"]   = true;   //-- samenvatting
  filter["liveweer"][0]["lv"]       = true;   //-- relatieve luchtvochtigheid
  filter["liveweer"][0]["windr"]    = true;   //-- windrichting
  filter["liveweer"][0]["windrgr"]  = false;  //-- windrichting in graden
  filter["liveweer"][0]["windms"]   = false;  //-- windkracht in m/s
  filter["liveweer"][0]["windbft"]  = true;   //-- windkracht in bft
  filter["liveweer"][0]["windknp"]  = false;  //-- windsnelheid in knoppen
  filter["liveweer"][0]["windkmh"]  = false;  //-- windsnelheid in km/h
  filter["liveweer"][0]["luchtd"]   = true;   //-- luchtdruk
  filter["liveweer"][0]["ldmmhg"]   = false;  //-- luchtdruk in mmHg
  filter["liveweer"][0]["dauwp"]    = true;   //-- dauwpunt
  filter["liveweer"][0]["zicht"]    = false;  //-- zicht in meters
  filter["liveweer"][0]["gr"]       = true;   //-- globale (zonne)straling in Watt/M2
  filter["liveweer"][0]["verw"]     = false;  //-- korte dagverwachting
  filter["liveweer"][0]["sup"]      = false;  //-- zon op
  filter["liveweer"][0]["sunder"]   = true;   //-- zon onder
  filter["liveweer"][0]["image"]    = true;   //-- afbeeldingsnaam
  filter["liveweer"][0]["alarm"]    = true;   //-- Geldt er een weerwaarschuwing? 1 = ja, 0 = nee
  filter["liveweer"][0]["lkop"]     = true;   //-- Weerwaarschuwing korte omschrijving
  filter["liveweer"][0]["ltekst"]   = false;  //-- Langere beschrijving van de waarschuwing
  filter["liveweer"][0]["wrschklr"] = false;  //-- KNMI kleurcode voor jouw regio
  filter["liveweer"][0]["wrsch_g"]  = false;  //-- Moment waarop de eerstkomende KNMI-waarschuwing geldt
  filter["liveweer"][0]["wrsch_gts"]= false;  //-- Timestamp van wrsch_g
  filter["liveweer"][0]["wrsch_gc"] = false;  //-- KNMI kleurcode voor de eerstkomende waarschuwing
  //---- week verwachting ---
  for(int i = 0; i < 4; i++)
  {
    filter["wk_verw"][i]["dag"]       = false;   //-- datum van deze dag
    filter["wk_verw"][i]["image"]     = false;   //-- afbeeldingsnaam
    filter["wk_verw"][i]["max_temp"]  = false;   //-- maximum temperatuur voor de dag
    filter["wk_verw"][i]["min_temp"]  = false;   //-- minimum temperatuur voor de dag
    filter["wk_verw"][i]["windbft"]   = false;   //--  windkracht in Beaufort
    filter["wk_verw"][i]["windkmh"]   = false;   //-- windkracht in km/h
    filter["wk_verw"][i]["windms"]    = false;   //-- windkracht in m/s
    filter["wk_verw"][i]["windknp"]   = false;   //-- windkracht in knopen
    filter["wk_verw"][i]["windrgr"]   = false;   //-- windrichting in graden
    filter["wk_verw"][i]["windr"]     = false;   //-- windrichting
    filter["wk_verw"][i]["neersl_perc_dag"] = false;   //-- kans op neerslag per dag
    filter["wk_verw"][i]["zond_perc_dag"]   = false;   //-- kans op zon per dag
  }

} //  configureFilters()

/**
 * Makes a request to the Weerlive API and processes the response.
 *
 * @return a pointer to a C-style string containing the processed weather data.
 *
 * @throws None.
 */
const char *Weerlive::request()
{
  int   weerliveStatus = 0;
  char  tmpMessage[30] = {};
  bool  gotData        = false;
  unsigned long requestTimeout = millis() + 10000; // 10 second timeout

  if (debug) debug->printf("weerlive::request() request() called\n");
  if (apiUrl.isEmpty())
  {
    weerliveText = "API URL not set";
    if (debug && doDebug) debug->println(weerliveText);
    return weerliveText.c_str();
  }

  if (debug && doDebug) debug->printf("\nrequest(): Weerlive request [%s]\n", apiHost);
  if (debug && doDebug) debug->printf("request(): Free Heap: [%d] bytes\n", ESP.getFreeHeap());
  if (debug && doDebug) debug->println(apiUrl);
  thisClient.setTimeout(2000);
  if (debug && doDebug) debug->printf("\nrequest(): Next: connect(%s, %d)\n\n", apiHost, 80);
  if (!thisClient.connect(apiHost, 80))
  {
    if (debug && doDebug) debug->printf("request(): connection to [%s] failed\n", apiHost);
    thisClient.flush();
    thisClient.stop();
    return "Error";
  }

  if (debug && doDebug) debug->printf("request(): connected to [%s]\n", apiHost);
  //-- This will send the request to the server
  if (debug && doDebug) debug->print(String("GET ") + apiUrl + " HTTP/1.1\r\n" +
               "Host: " + apiHost + "\r\n" +
               "Connection: close\r\n\r\n");
  thisClient.print(String("GET ") + apiUrl + " HTTP/1.1\r\n" +
                   "Host: " + apiHost + "\r\n" +
                   "Connection: close\r\n\r\n");
  delay(100);

  // Clear the buffer before reading new data
  memset(jsonResponse, 0, sizeof(jsonResponse));
  
  while ((thisClient.connected() || thisClient.available()) && !gotData && millis() < requestTimeout)
  {
    yield();
    if (thisClient.available())
    {
      if (debug && doDebug) debug->println("weerlive::request(): looking for HTTP/1.1 ..");
      //--- skip to find HTTP/1.1
      //--- then parse response code
      if (thisClient.find("HTTP/1.1"))
      {
        if (debug && doDebug) debug->println("weerlive::request(): found HTTP/1.1 .. continue");
        //-- parse status code
        weerliveStatus = thisClient.parseInt();
        if (debug && doDebug) debug->printf("weerlive::request() plain weerliveStatus[%d]\n", weerliveStatus);
        snprintf(tmpMessage, sizeof(tmpMessage), "\nweerlive::request(): Statuscode: [%d]\n", weerliveStatus);
        if (debug && doDebug) debug->println(tmpMessage);
        if (weerliveStatus != 200)
        {
          if (debug && doDebug) debug->println("weerlive::request() ERROR!\n");
          thisClient.flush();
          thisClient.stop();
          return tmpMessage;
        }
        if (debug && doDebug) debug->println("weerlive::request() OK!\n");
      }
      else
      {
        if (debug && doDebug) debug->println("weerlive::request() Error reading weerLive.. -> bailout!\n");
        thisClient.flush();
        thisClient.stop();
        return "weerlive::request() Error reading Weerlive -> bailout";
      }
      //--- skip headers
      if (debug && doDebug) debug->println("weerlive::request() looking for headers ..");
      if (thisClient.find("\r\n\r\n"))
      {
        if (debug && doDebug) debug->println("weerlive::request() found headers .. continue");
        
        // Read the JSON response with timeout
        int charsRead = 0;
        int maxChars = sizeof(jsonResponse) - 2; // Leave space for null terminator
        unsigned long readTimeout = millis() + 5000; // 5 second read timeout
        
        // Find the opening brace
        if (thisClient.find("{")) {
          // Put the opening brace at the beginning of our buffer
          jsonResponse[0] = '{';
          charsRead = 1;
          
          // Read the rest of the JSON data
          while (thisClient.available() && charsRead < maxChars && millis() < readTimeout) {
            char c = thisClient.read();
            jsonResponse[charsRead++] = c;
            
            // If we find the final closing brace of the JSON object, we're done
            if (c == '}' && thisClient.peek() < 0) {
              break;
            }
            yield(); // Allow other tasks to run
          }
          
          // Ensure null termination
          jsonResponse[charsRead] = '\0';
          
          if (charsRead > (sizeof(jsonResponse)-5)) 
          {
            if (debug) debug->println("weerlive::request() Error: JSON data too large\n");
            if (debug) debug->printf("weerlive::request(): Read %d bytes of JSON data (Max. is %d bytes)\n"
                                                                  , charsRead, (sizeof(jsonResponse)-3));
            thisClient.flush();
            thisClient.stop();
            return "weerlive::request() Error: JSON data too large for jsonResponse buffer";
          }
          // Check if we have a complete JSON response
          // Look for the last closing brace and bracket
          if (charsRead > 5) {
            bool foundClosing = false;
            for (int i = charsRead - 1; i >= 0; i--) {
              if (jsonResponse[i] == '}') {
                // Make sure we have a proper ending
                jsonResponse[i+1] = '\0';
                foundClosing = true;
                break;
              }
            }
            
            if (!foundClosing) {
              // If we didn't find a proper closing, add one
              if (charsRead < maxChars - 2) {
                jsonResponse[charsRead] = '}';
                jsonResponse[charsRead+1] = '\0';
              }
            }
          }
          
          gotData = true;
        } else {
          if (debug && doDebug) debug->println("weerlive::request() Error: Could not find JSON start\n");
        }
      }
    }
    
    // Prevent getting stuck in the loop
    if (millis() > requestTimeout) {
      if (debug && doDebug) debug->println("weerlive::request() Error: Request timed out\n");
      break;
    }
  }

  thisClient.flush();
  thisClient.stop();

  if (!gotData) {
    weerliveText = "Error: Failed to get complete data from server";
    if (debug && doDebug) debug->println(weerliveText);
    return weerliveText.c_str();
  }

  if (weerliveStatus == 200)
  {
#ifdef DEBUG
    if (debug && doDebug) debug->printf("\nrequest(): jsonResponse ==[%d]===\n%s\nrequest(): =============\n\n", strlen(jsonResponse), jsonResponse);
#endif  // DEBUG
    if (strlen(jsonResponse) == 0)
    {
      if (debug && doDebug) debug->println(" ... Bailout!!\n");
      return weerliveText.c_str();
    }

    DeserializationError error = deserializeJson(doc, jsonResponse, DeserializationOption::Filter(filter));
    if (debug && doDebug) debug->printf("request(): After deserializeJson(): Free Heap: [%d] bytes\n", ESP.getFreeHeap());

    if (error)
    {
      weerliveText = "request(): WeerliveClass: deserializeJson() failed: " + String(error.f_str());
      if (debug && doDebug) debug->println(weerliveText);
      if (debug && doDebug) debug->printf("request(): Free Heap: [%d] bytes\n", ESP.getFreeHeap());
      return weerliveText.c_str();
    }

    weerliveText = "";

    if (debug && doDebug) debug->println("\nrequest(): Processliveweer ......  \n");
    JsonArray liveweerArray = doc["liveweer"];
    if (debug && doDebug) debug->printf("request(): After liveweerArray: Free Heap [%d] bytes\n", ESP.getFreeHeap());
    alarmInd = -1;
    for (JsonObject weather : liveweerArray)
    {
      for (JsonPair kv : weather)
      {
#ifdef DEBUG
        if (debug && doDebug) debug->print(kv.key().c_str());
        if (debug && doDebug) debug->print(": ");
        if (debug && doDebug) debug->println(kv.value().as<String>());
#endif  // DEBUG
        if (kv.key() == "plaats")
        {
          weerliveText += kv.value().as<String>();
        }
        else if (kv.key() == "time")
        {
          weerliveText += " " + kv.value().as<String>();
        }
        else if (kv.key() == "temp")
        {
          weerliveText += " " + kv.value().as<String>() + "°C";
        }
        else if (kv.key() == "gtemp")
        {
          weerliveText += " gevoelstemperatuur " + kv.value().as<String>() + "°C";
        }
        else if (kv.key() == "samenv")
        {
          weerliveText += " " + kv.value().as<String>();
        }
        else if (kv.key() == "lv")
        {
          weerliveText += " luchtvochtigheid " + kv.value().as<String>() + "%";
        }
        else if (kv.key() == "windr")
        {
          weerliveText += " windrichting " + kv.value().as<String>();
        }
        else if (kv.key() == "windrgr")
        {
          weerliveText += " windrichting " + kv.value().as<String>() + "°";
        }
        else if (kv.key() == "windms")
        {
          weerliveText += " " + kv.value().as<String>() + " m/s";
        }
        else if (kv.key() == "windbft")
        {
          weerliveText += " " + kv.value().as<String>() + " bft";
        }
        else if (kv.key() == "windknp")
        {
          weerliveText += " " + kv.value().as<String>() + " kts";
        }
        else if (kv.key() == "windkmh")
        {
          weerliveText += " " + kv.value().as<String>() + " km/h";
        }
        else if (kv.key() == "luchtd")
        {
          weerliveText += " luchtdruk " + kv.value().as<String>() + " hPa";
        }
        else if (kv.key() == "ldmmhg")
        {
          weerliveText += " luchtdruk " + kv.value().as<String>() + " mmHg";
        }
        else if (kv.key() == "dauwp")
        {
          weerliveText += " dauwpunt " + kv.value().as<String>() + "°C";
        }
        else if (kv.key() == "zicht")
        {
          weerliveText += " zicht " + kv.value().as<String>() +" m";
        }
        else if (kv.key() == "gr")
        {
          weerliveText += " globale (zonne)straling " + kv.value().as<String>() + " Watt/M2";
        }
        else if (kv.key() == "verw")
        {
          weerliveText += " dagverwachting: " + kv.value().as<String>();
        }
        else if (kv.key() == "sup")
        {
          weerliveText += " zon op " + kv.value().as<String>();
        }
        else if (kv.key() == "sunder")
        {
          weerliveText += " zon onder " + kv.value().as<String>();
        }
        else if (kv.key() == "image")
        {
          weerliveText += " " + kv.value().as<String>();
        }
        else if (kv.key() == "alarm")
        {
          alarmInd = kv.value().as<String>().toInt();
        }
        else if (kv.key() == "lkop" && (alarmInd == 1))
        {
          weerliveText += " waarschuwing: " + kv.value().as<String>();
        }
        else if (kv.key() == "ltekst" && (alarmInd == 1))
        {
          weerliveText += " waarschuwing: " + kv.value().as<String>();
        }
        else if (kv.key() == "wrschklr")
        {
          weerliveText += " KNMI kleurcode " + kv.value().as<String>();
        }
        else if (kv.key() == "wrsch_g"&& (alarmInd == 1))
        {
          weerliveText += " waarschuwing:" + kv.value().as<String>();
        }
        else if (kv.key() == "wrsch_gts" && (alarmInd == 1))
        {
          weerliveText += " Timestamp van waarschuwing " + kv.value().as<String>();
        }
        else if (kv.key() == "wrsch_gc" && (alarmInd == 1))
        {
          weerliveText += " kleurcode eerstkomende waarschuwing " + kv.value().as<String>();
        }
        else
        {
          //weerliveText += ", [" + String(kv.key().c_str()) + "] = " + kv.value().as<String>();
        }
      }
    }
    
    // Use debug stream instead of direct Serial output
    if (debug) {
      serializeJsonPretty(liveweerArray, *debug);
      debug->println();
    }
    
    if (debug && doDebug) debug->println("\nrequest(): Process wk_verw ......  \n");
    // Process the wk_verw array
    JsonArray wk_verwArray = doc["wk_verw"];
    if (debug && doDebug) debug->printf("request(): After wk_verwArray: Free Heap [%d] bytes\n\n", ESP.getFreeHeap());
    for (JsonVariant v : wk_verwArray)
    {
      JsonObject obj = v.as<JsonObject>();
      for (JsonPair kv : obj)
      {
#ifdef DEBUG
        if (debug && doDebug) debug->print(kv.key().c_str());
        if (debug && doDebug) debug->print(": ");
        if (debug && doDebug) debug->println(kv.value().as<String>());
#endif  // DEBUG

        if (kv.key() == "dag" && filter["wk_verw"][0]["dag"])
        {
          weerliveText += " | "+ String(dateToDayName(kv.value().as<String>().c_str()));
        }
        else if (kv.key() == "image" )
        {
          weerliveText += " " + kv.value().as<String>();
        }
        else if (kv.key() == "max_temp" )
        {
          weerliveText += " max " + kv.value().as<String>() + "°C";
        }
        else if (kv.key() == "min_temp" )
        {
          weerliveText += " min " + kv.value().as<String>() + "°C";
        }
        else if (kv.key() == "windbft")
        {
          weerliveText += " " + kv.value().as<String>() + " bft";
        }
        else if (kv.key() == "windkmh")
        {
          weerliveText += " " + kv.value().as<String>() + " km/h";
        }
        else if (kv.key() == "windms")
        {
          weerliveText += " " + kv.value().as<String>() + " m/s";
        }
        else if (kv.key() == "windknp")
        {
          weerliveText += " " + kv.value().as<String>() + " kts";
        }
        else if (kv.key() == "windrgr")
        {
          weerliveText += " windrichting " + kv.value().as<String>() + "°";
        }
        else if (kv.key() == "windr")
        {
          weerliveText += " windrichting " + kv.value().as<String>();
        }
        else if (kv.key() == "neersl_perc_dag")
        {
          weerliveText += " neerslag kans " + kv.value().as<String>() + "%";
        }
        else if (kv.key() == "zond_perc_dag")
        {
          weerliveText += " zon kans " + kv.value().as<String>() + "%";
        }
        else
        {
          //weerliveText += ", [" + String(kv.key().c_str()) + "] = " + kv.value().as<String>();
        }
      }
    }
#ifdef DEBUG
    if (debug && doDebug) debug->printf("\nrequest(): weerliveText is [%s]\n\n", weerliveText.c_str());
#endif  // DEBUG
  }
  else
  {
    weerliveText = "Error [" + String(weerliveStatus) + "] on HTTP request";
    if (debug && doDebug) debug->println(weerliveText);
#ifdef DEBUG
    if (debug && doDebug) debug->printf("[HTTP] GET... code: %d\n", weerliveStatus);
#endif  // DEBUG
    if (weerliveStatus == 429)
    {
      weerliveText = "request(): WeerliveClass: status [429] Too many requests";
#ifdef DEBUG
      if (debug && doDebug) debug->println(weerliveText);
#endif  // DEBUG
      return weerliveText.c_str();
    }
  }

  return weerliveText.c_str();
} //  request()


/**
 * Takes a date string in the format "day-month-year" and converts it to the
 * corresponding day of the week using Zeller's Congruence
 *
 * @param date_str The date string in the format "day-month-year"
 *
 * @return A const char* representing the day of the week
 *
 * @throws None
 */
const char *Weerlive::dateToDayName(const char *date_str)
{
  int day, month, year;
  sscanf(date_str, "%d-%d-%d", &day, &month, &year);

  if (month < 3)
  {
    month += 12;
    year -= 1;
  }

  int K = year % 100;
  int J = year / 100;
  int f = day + 13 * (month + 1) / 5 + K + K / 4 + J / 4 + 5 * J;
  int dayOfWeek = f % 7;

  // Adjust dayOfWeek to fit our dayNames array (0 = Saturday, 1 = Sunday, ..., 6 = Friday)
  dayOfWeek = (dayOfWeek + 6) % 7;

  return dayNames[dayOfWeek];
}


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