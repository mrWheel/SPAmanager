#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFiManager.h>
#include "SPAmanager.h"

#define CLOCK_UPDATE_INTERVAL  1000

SPAmanager spa(80);

uint32_t lastCounterUpdate = 0;

const char *hostName = "basicDM";
uint32_t  counter = 0;
bool counterRunning = false;


    
void mainCallback1()
{
    spa.setErrorMessage("Main Menu \"Counter\" clicked!", 5);
    spa.activatePage("CounterPage");

} // mainCallback1()
    
void mainCallback2()
{
    spa.setErrorMessage("Main Menu \"Input\" clicked!", 5);
    spa.activatePage("InputPage");

} // mainCallback2()

void startCounterCallback()
{
    spa.setMessage("Counter: Start clicked!", 3);
    spa.enableMenuItem("CounterPage", "Counter", "Stop");
    spa.disableMenuItem("CounterPage", "Counter", "Reset");
    spa.disableMenuItem("CounterPage", "Counter", "Start");
    counterRunning = true;
    spa.setPlaceholder("CounterPage", "counterState", "Started");

} // startCounterCallback()

void stopCounterCallback()
{
    spa.setMessage("Counter: Stop clicked!", 3);
    spa.disableMenuItem("CounterPage","Counter", "Stop");
    spa.enableMenuItem("CounterPage","Counter", "Start");
    spa.enableMenuItem("CounterPage","Counter", "Reset");
    counterRunning = false;
    spa.setPlaceholder("CounterPage", "counterState", "Stopped");

} // stopCounterCallback()

void resetCounterCallback()
{
    spa.setMessage("Counter: Reset clicked!", 3);
    counterRunning = false;
    counter = 0;
    spa.setPlaceholder("CounterPage", "counterState", "Reset");
    spa.setPlaceholder("CounterPage", "counter", counter);

} // resetCounterCallback()

void exitCounterCallback()
{
    spa.setMessage("Counter: \"Exit\" clicked!", 10);
    spa.activatePage("Main");

} // exitCounterCallback()


void handleInputMenu(const char* param)
{
  if (strcmp(param, "1") == 0) 
  {
      spa.setMessage("InputPage: Initialize Input!", 3);
      spa.setPlaceholder("InputPage", "input1", 12345);
      spa.setPlaceholder("InputPage", "input2", "TextString");
      spa.setPlaceholder("InputPage", "input3", 123.45);
      int counter = spa.getPlaceholder("CounterPage", "counter").asInt();
      spa.setPlaceholder("InputPage", "counter", counter);
  }
  else if (strcmp(param, "2") == 0) 
  {
      spa.setMessage("InputTest: save Input!", 1);
      int input1 = spa.getPlaceholder("InputPage", "input1").asInt();
      Serial.printf("input1: [%d]\n", input1);
      char buff[100] = {};
      snprintf(buff, sizeof(buff), "%s", spa.getPlaceholder("InputPage", "input2").c_str());
      Serial.printf("input2: [%s]\n", buff);
      float input3 = spa.getPlaceholder("InputPage", "input3").asFloat();
      Serial.printf("input3: [%f]\n", input3); 
      int counter = spa.getPlaceholder("CounterPage", "counter").asInt();
      spa.setPlaceholder("InputPage", "counter", counter);
      Serial.printf("counter: [%d]\n", counter);
  }
  else if (strcmp(param, "3") == 0) 
  {
      spa.setMessage("InputTest: Exit Input!", 3);
      spa.activatePage("Main");
  }

} // handleInputMenu()

void setupMainPage()
{
    spa.addPage("Main", "<div style='font-size: 48px; text-align: center; font-weight: bold;'>basicDM page</div>");
    
    spa.setPageTitle("Main", "Single Page Application Example");
    //-- Add Main menu
    spa.addMenu("Main", "Main Menu");
    spa.addMenuItem("Main", "Main Menu", "Counter", mainCallback1);
    spa.addMenuItem("Main", "Main Menu", "InputTest", mainCallback2);
    spa.addMenuItem("Main", "Main Menu", "Item3", "/");

} // setupMainPage()

void setupCounterPage()
{
    const char *counterPage = R"(
    <div id='counterState' style='font-size: 30px; text-align: center; font-weight: bold;'></div>
    <div id='counter' style='font-size: 48px; text-align: right; font-weight: bold;'>0</div>)";
  
    spa.addPage("CounterPage", counterPage);
    spa.setPageTitle("CounterPage", "Counter");
    //-- Add Counter menu
    spa.addMenu("CounterPage", "Counter");
    spa.addMenuItem("CounterPage", "Counter", "Start", startCounterCallback);
    spa.addMenuItem("CounterPage", "Counter", "Stop",  stopCounterCallback);
    spa.addMenuItem("CounterPage", "Counter", "Reset", resetCounterCallback);
    spa.addMenuItem("CounterPage", "Counter", "Exit",  exitCounterCallback);

    spa.disableMenuItem("CounterPage", "Counter", "Reset");
    spa.disableMenuItem("CounterPage", "Counter", "Stop");

    spa.setPlaceholder("CounterPage", "counterState", "Stopped");

} // setupCounterPage()


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

        <label for="counter">Counter:</label>
        <input type="number" step="1" id="counter" placeholder="CounterValue" disabled>
        <br>
    </form>
    )";
  
    spa.addPage("InputPage", inputPage);
    spa.setPageTitle("InputPage", "InputTest");
    //-- Add InputPage menu
    spa.addMenu("InputPage", "InputTest");
    spa.addMenuItem("InputPage", "InputTest", "Initialize", handleInputMenu, "1");
    spa.addMenuItem("InputPage", "InputTest", "Save",  handleInputMenu, "2");
    spa.addMenuItem("InputPage", "InputTest", "Exit",  handleInputMenu, "3");

} // setupInputPage()


void updateCounter() 
{
    if (millis() - lastCounterUpdate >= CLOCK_UPDATE_INTERVAL) 
    {
        if (counterRunning) 
        {
            counter++;
            spa.setPlaceholder("CounterPage", "counter", counter);
            lastCounterUpdate = millis();
        }
    }
} // updateCounter()

void setup()
{
    Serial.begin(115200);
    delay(3000);
    LittleFS.begin();
    
    //-- Connect to WiFi
    WiFiManager wifiManager;
    Serial.println("Attempting WiFi connection...");
    wifiManager.autoConnect(hostName);
    
    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    spa.begin("/SYS", &Serial);
    setupMainPage();
    setupCounterPage();
    setupInputPage();
    spa.activatePage("Main");
    
    Serial.println("Done with setup() ..\n");

} // setup()


void loop()
{
    spa.server.handleClient();
    spa.ws.loop();
    updateCounter();

} // loop()
