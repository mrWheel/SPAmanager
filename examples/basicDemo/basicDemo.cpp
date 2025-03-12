#include <Arduino.h>

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFiManager.h>
#include "displayManager.h"

#define CLOCK_UPDATE_INTERVAL  1000

DisplayManager dm(80);

uint32_t lastCounterUpdate = 0;

const char *hostName = "basicDM";
uint32_t  counter = 0;
bool counterRunning = false;


void vDelay(uint delayMs) 
{
  uint32_t previousMillis = millis();
  
  while (millis() - previousMillis < delayMs) 
  {
    yield();
  }
}
    
void mainCallback1()
{
    dm.setErrorMessage("Main Menu \"Counter\" clicked!", 5);
    dm.activatePage("CounterPage");
}
    
void mainCallback2()
{
    dm.setErrorMessage("Main Menu \"Input\" clicked!", 5);
    dm.activatePage("InputPage");
}

void startCounterCallback()
{
    dm.setMessage("Counter: Start clicked!", 3);
    dm.enableMenuItem("CounterPage", "StopWatch", "Stop");
    dm.disableMenuItem("CounterPage", "StopWatch", "Reset");
    dm.disableMenuItem("CounterPage", "StopWatch", "Start");
    counterRunning = true;
    dm.setPlaceholder("CounterPage", "counterState", "Started");
}

void stopCounterCallback()
{
    dm.setMessage("Counter: Stop clicked!", 3);
    dm.disableMenuItem("CounterPage","StopWatch", "Stop");
    dm.enableMenuItem("CounterPage","StopWatch", "Start");
    dm.enableMenuItem("CounterPage","StopWatch", "Reset");
    counterRunning = false;
    dm.setPlaceholder("CounterPage", "counterState", "Stopped");
}

void resetCounterCallback()
{
    dm.setMessage("Counter: Reset clicked!", 3);
    counterRunning = false;
    counter = 0;
    dm.setPlaceholder("CounterPage", "counterState", "Reset");
    dm.setPlaceholder("CounterPage", "counter", counter);
}

void exitCounterCallback()
{
    dm.setMessage("Counter: \"Exit\" clicked!", 10);
    dm.activatePage("Main");
}

/*** 
void initInputCallback()
{
    dm.setMessage("InputPage: Initialize Input!", 3);
    dm.setPlaceholder("InputPage", "input1", 12345);
    dm.setPlaceholder("InputPage", "input2", "TextString");
    dm.setPlaceholder("InputPage", "input3", 123.45);
    int counter = dm.getPlaceholder("CounterPage", "counter").asInt();
    dm.setPlaceholder("InputPage", "counter", counter);
}

void saveInputCallback()
{
    dm.setMessage("InputTest: save Input!", 1);
    int input1 = dm.getPlaceholder("InputPage", "input1").asInt();
    Serial.printf("input1: [%d]\n", input1);
    char buff[100] = {};
    snprintf(buff, sizeof(buff), "%s", dm.getPlaceholder("InputPage", "input2").c_str());
    Serial.printf("input2: [%s]\n", buff);
    float input3 = dm.getPlaceholder("InputPage", "input3").asFloat();
    Serial.printf("input3: [%f]\n", input3); 
    int counter = dm.getPlaceholder("CounterPage", "counter").asInt();
    dm.setPlaceholder("InputPage", "counter", counter);
    Serial.printf("counter: [%d]\n", counter);
}

***/

void handleInputMenu(uint8_t param)
{
  switch (param)
  {
    case 1: {
              dm.setMessage("InputPage: Initialize Input!", 3);
              dm.setPlaceholder("InputPage", "input1", 12345);
              dm.setPlaceholder("InputPage", "input2", "TextString");
              dm.setPlaceholder("InputPage", "input3", 123.45);
              int counter = dm.getPlaceholder("CounterPage", "counter").asInt();
              dm.setPlaceholder("InputPage", "counter", counter);
            }
            break;
    case 2: {
              dm.setMessage("InputTest: save Input!", 1);
              int input1 = dm.getPlaceholder("InputPage", "input1").asInt();
              Serial.printf("input1: [%d]\n", input1);
              char buff[100] = {};
              snprintf(buff, sizeof(buff), "%s", dm.getPlaceholder("InputPage", "input2").c_str());
              Serial.printf("input2: [%s]\n", buff);
              float input3 = dm.getPlaceholder("InputPage", "input3").asFloat();
              Serial.printf("input3: [%f]\n", input3); 
              int counter = dm.getPlaceholder("CounterPage", "counter").asInt();
              dm.setPlaceholder("InputPage", "counter", counter);
              Serial.printf("counter: [%d]\n", counter);
            }
            break;
    case 3: {
              dm.setMessage("InputTest: Exit Input!", 3);
              dm.activatePage("Main");
            }
            break;
  }
}

void setupMainPage()
{
    dm.addPage("Main", "<div style='font-size: 48px; text-align: center; font-weight: bold;'>basicDM page</div>");
    
    dm.setPageTitle("Main", "Display Manager Example");
    //-- Add Main menu
    dm.addMenu("Main", "Main Menu");
    dm.addMenuItem("Main", "Main Menu", "StopWatch", mainCallback1);
    dm.addMenuItem("Main", "Main Menu", "InputTest", mainCallback2);
    dm.addMenuItem("Main", "Main Menu", "Item3", "/");
}

void setupCounterPage()
{
    const char *counterPage = R"(
    <div id='counterState' style='font-size: 30px; text-align: center; font-weight: bold;'></div>
    <div id='counter' style='font-size: 48px; text-align: right; font-weight: bold;'>0</div>)";
  
    dm.addPage("CounterPage", counterPage);
    dm.setPageTitle("CounterPage", "StopWatch");
    //-- Add Counter menu
    dm.addMenu("CounterPage", "StopWatch");
    dm.addMenuItem("CounterPage", "StopWatch", "Start", startCounterCallback);
    dm.addMenuItem("CounterPage", "StopWatch", "Stop",  stopCounterCallback);
    dm.addMenuItem("CounterPage", "StopWatch", "Reset", resetCounterCallback);
    dm.addMenuItem("CounterPage", "StopWatch", "Exit",  exitCounterCallback);

    dm.disableMenuItem("CounterPage", "StopWatch", "Reset");
    dm.disableMenuItem("CounterPage", "StopWatch", "Stop");

    dm.setPlaceholder("CounterPage", "counterState", "Stopped");
}

void setupInputPage()
{
    const char *inputPage = R"(
    <form>
        <label for="input1">Input 1:</label>
        <input type="number" step="1" id="input1" placeholder="integer value">
        <br>
        
        <label for="input2">Input 2:</label>
        <input type="text" id="input2" placeholder="Enter text value">
        <br>
        
        <label for="input3">Input 3:</label>
        <input type="number" step="any" id="input3" placeholder="Enter float value">
        <br>
        <br>

        <label for="counter">StopWatch:</label>
        <input type="number" step="1" id="counter" placeholder="CounterValue" disabled>
        <br>
    </form>
    )";
  
    dm.addPage("InputPage", inputPage);
    dm.setPageTitle("InputPage", "InputTest");
    //-- Add InputPage menu
    dm.addMenu("InputPage", "InputTest");
    dm.addMenuItem("InputPage", "InputTest", "Initialize", handleInputMenu, 1);
    dm.addMenuItem("InputPage", "InputTest", "Save",  handleInputMenu, 2);
    dm.addMenuItem("InputPage", "InputTest", "Exit",  handleInputMenu, 3);
}

void updateCounter() 
{
    if (millis() - lastCounterUpdate >= CLOCK_UPDATE_INTERVAL) 
    {
        if (counterRunning) 
        {
            counter++;
            dm.setPlaceholder("CounterPage", "counter", counter);
            lastCounterUpdate = millis();
        }
    }
}

void setup()
{
    Serial.begin(115200);
    delay(3000);

    //-- Connect to WiFi
    WiFiManager wifiManager;
    Serial.println("Attempting WiFi connection...");
    wifiManager.autoConnect(hostName);
    
    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    dm.begin(&Serial);
    setupMainPage();
    setupCounterPage();
    setupInputPage();
    dm.activatePage("Main");
    
    Serial.println("Done with setup() ..\n");
}

void loop()
{
    dm.server.handleClient();
    dm.ws.loop();
    updateCounter();
}
