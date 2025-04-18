#include <Arduino.h>

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFiManager.h>
#include <FS.h>
#include <LittleFS.h>
#include <Networking.h>
#include <FSmanager.h>
#include <string>
#include "SPAmanager.h"

#define CLOCK_UPDATE_INTERVAL  1000

Networking* network = nullptr;
Stream* debug = nullptr;

SPAmanager spa(80);
//WebServer server(80);
//-- we need to use the server from the SPAmanager!!
FSmanager fsManager(spa.server);

uint32_t lastCounterUpdate = 0;

const char *hostName = "extendedDemo";
uint32_t  counter = 0;
bool counterRunning = false;



void pageIsLoadedCallback()
{
  debug->println("pageIsLoadedCallback(): Page is loaded callback executed");
  debug->println("pageIsLoadedCallback(): Nothing to do!");
} 

    
void mainCallback1()
{
    spa.setErrorMessage("Main Menu \"Counter\" clicked!", 5);
    spa.activatePage("CounterPage");
}
    
void mainCallback2()
{
    spa.setErrorMessage("Main Menu \"Input\" clicked!", 5);
    spa.activatePage("InputPage");
}


void exitCounterCallback()
{
    spa.setMessage("Counter: \"Exit\" clicked!", 10);
    spa.setPopupMessage("Counter: \"Exit\" clicked!", 5);
    spa.activatePage("Main");
}



void mainFSCallbackStart()
{
    //spa.setMessage("Main Menu \"FSmanager\" clicked!", 10);
    spa.setPopupMessage("Main Menu \"FSmanager\" clicked!", 10);
    spa.activatePage("FSmanagerPage");
    spa.callJsFunction("loadFileList");
}

void handleFSexitCallback()
{
    spa.setMessage("FS Manager : Exit Clicked!", 5);
    spa.setPopupMessage("FSmanager \"Exit\" clicked!", 0);
    spa.activatePage("Main");
}


void processInputCallback(const std::map<std::string, std::string>& inputValues)
{
  debug->println("Process callback: proceed action received");
  debug->printf("Received %d input values\n", inputValues.size());
  
  // Access input values directly from the map
  if (inputValues.count("input1") > 0) 
  {
    const std::string& value = inputValues.at("input1");
    debug->printf("Input1 (raw): %s\n", value.c_str());
    
    // Convert to integer if needed
    int intValue = atoi(value.c_str());
    debug->printf("Input1 (as int): %d\n", intValue);
  } else 
  {
    debug->println("Input1 not found in input values");
  }
  
  if (inputValues.count("input2") > 0) 
  {
    const std::string& value = inputValues.at("input2");
    debug->printf("Input2: %s\n", value.c_str());
  } else 
  {
    debug->println("Input2 not found in input values");
  }
  
  // Print all input values for debugging
  debug->println("All input values:");
  for (const auto& pair : inputValues) 
  {
    debug->printf("  %s = %s\n", pair.first.c_str(), pair.second.c_str());
  }
}
void processUploadFileCallback()
{
  debug->println("Process processUploadFileCallback(): proceed action received");
}


void doJsFunction()
{
    spa.setMessage("Main Menu \"isFSmanagerLoaded\" clicked!", 5);
    spa.callJsFunction("isFSmanagerLoaded");
}


void handleMenuItem(const char* param)
{
  if (strcmp(param, "Input-1") == 0) 
  {
    spa.setMessage("InputPage: Initialize Input!", 3);
    spa.setPlaceholder("InputPage", "input1", 12345);
    spa.setPlaceholder("InputPage", "input2", "TextString");
    spa.setPlaceholder("InputPage", "input3", 123.45);
    int counter = spa.getPlaceholder("CounterPage", "counter").asInt();
    spa.setPlaceholder("InputPage", "counter", counter);
  }
  else if (strcmp(param, "Input-2") == 0) 
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
  else if (strcmp(param, "Input-3") == 0) 
  {
    spa.setMessage("InputTest: Exit Input!", 3);
    spa.activatePage("Main");
  }
  else if (strcmp(param, "Counter-1") == 0) 
  {
    spa.setMessage("Counter: Start clicked!", 3);
    spa.enableMenuItem("CounterPage", "StopWatch", "Stop");
    spa.disableMenuItem("CounterPage", "StopWatch", "Reset");
    spa.disableMenuItem("CounterPage", "StopWatch", "Start");
    counterRunning = true;
    spa.setPlaceholder("CounterPage", "counterState", "Started");
  }
  else if (strcmp(param, "Counter-2") == 0) 
  {
    spa.setMessage("Counter: Stop clicked!", 3);
    spa.disableMenuItem("CounterPage","StopWatch", "Stop");
    spa.enableMenuItem("CounterPage","StopWatch", "Start");
    spa.enableMenuItem("CounterPage","StopWatch", "Reset");
    counterRunning = false;
    spa.setPlaceholder("CounterPage", "counterState", "Stopped");
  }
  else if (strcmp(param, "Counter-3") == 0) 
  {
    spa.setMessage("Counter: Reset clicked!", 3);
    counterRunning = false;
    counter = 0;
    spa.setPlaceholder("CounterPage", "counterState", "Reset");
    spa.setPlaceholder("CounterPage", "counter", counter);
  }
  else if (strcmp(param, "FSM-1") == 0) 
  {
      spa.setMessage("FS Manager : List LittleFS Clicked!", 5);
      spa.disableID("FSmanagerPage", "fsm_addFolder");
      spa.disableID("FSmanagerPage", "fsm_fileUpload");
      spa.enableID("FSmanagerPage",  "fsm_fileList");
      spa.callJsFunction("loadFileList");
  } 
  else if (strcmp(param, "FSM-4") == 0) 
  {
      spa.setMessage("FS Manager : Exit Clicked!", 5);
      spa.activatePage("Main");
  }
} //  handleMenuItem()

void setupMainPage()
{
    const char *mainPage = R"HTML(
    <div style="font-size: 48px; text-align: center; font-weight: bold;">Extended Demo Page</div>
    )HTML";
    
    spa.addPage("Main", mainPage);
    spa.setPageTitle("Main", "Single Page Apllication");

    //-- Add Main menu
    spa.addMenu("Main", "Main Menu");
    spa.addMenuItem("Main", "Main Menu", "StopWatch", mainCallback1);
    spa.addMenuItem("Main", "Main Menu", "InputTest", mainCallback2);
    spa.addMenuItem("Main", "Main Menu", "FSmanager", mainFSCallbackStart);
    spa.addMenuItem("Main", "Main Menu", "isFSmanagerLoaded", doJsFunction);
    spa.addMenu("Main", "TestPopUp");
    const char *popup5Input = R"HTML(
      <div style="font-size: 48px; text-align: center; font-weight: bold;">Five Input Fields</div>
      <label for="input1">Input 1 (Number):</label>
      <input type="number" step="1" id="input1" placeholder="integer value">
      <br>
      <label for="input2">Input 2 (Text):</label>
      <input type="text" id="input2" placeholder="text value">
      <br>
      <label for="input3">Input 3 (Float):</label>
      <input type="number" step="0.1" id="input3" placeholder="float value">
      <br>
      <label for="input4">Input 4 (Date):</label>
      <input type="date" id="input4">
      <br>
      <label for="input5">Input 5 (Color):</label>
      <input type="color" id="input5" value="#ff0000">
      <br>
      <button type="button" onClick="closePopup('popup_TestPopUp_InputFields')">Cancel</button>
      <button type="button" id="proceedButton" onClick="processAction('proceed')">Proceed</button>

    )HTML";
    spa.addMenuItemPopup("Main", "TestPopUp", "InputFields5", popup5Input, processInputCallback);

    const char *popup2Input = R"HTML(
      <div style="font-size: 48px; text-align: center; font-weight: bold;">Two Input Fields</div>
      <label for="input1">Input 1 (Number):</label>
      <input type="number" step="1" id="input1" placeholder="integer value">
      <br>
      <label for="input2">Input 2 (Text):</label>
      <input type="text" id="input2" placeholder="text value">
      <br>
      <button type="button" onClick="closePopup('popup_TestPopUp_InputFields')">Cancel</button>
      <button type="button" id="proceedButton" onClick="processAction('proceed')">Proceed</button>
    )HTML";
    spa.addMenuItemPopup("Main", "TestPopUp", "InputFields", popup2Input, processInputCallback);

}

void setupCounterPage()
{
    const char *counterPage = R"HTML(
    <div id="counterState" style="font-size: 30px; text-align: center; font-weight: bold;"></div>
    <div id="counter" style="font-size: 48px; text-align: right; font-weight: bold;">0</div>
    )HTML";
  
    spa.addPage("CounterPage", counterPage);
    spa.setPageTitle("CounterPage", "StopWatch");
    //-- Add Counter menu
    spa.addMenu("CounterPage", "StopWatch");
    spa.addMenuItem("CounterPage", "StopWatch", "Start", handleMenuItem, "Counter-1");
    spa.addMenuItem("CounterPage", "StopWatch", "Stop",  handleMenuItem, "Counter-2");
    spa.addMenuItem("CounterPage", "StopWatch", "Reset", handleMenuItem, "Counter-3");
    spa.addMenuItem("CounterPage", "StopWatch", "Exit",  exitCounterCallback);

    spa.disableMenuItem("CounterPage", "StopWatch", "Reset");
    spa.disableMenuItem("CounterPage", "StopWatch", "Stop");

    spa.setPlaceholder("CounterPage", "counterState", "Stopped");
}

void setupInputPage()
{
    const char *inputPage = R"HTML(
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
    )HTML";
  
    spa.addPage("InputPage", inputPage);
    spa.setPageTitle("InputPage", "InputTest");
    //-- Add InputPage menu
    spa.addMenu("InputPage", "InputTest");
    spa.addMenuItem("InputPage", "InputTest", "Initialize", handleMenuItem, "Input-1");
    spa.addMenuItem("InputPage", "InputTest", "Save",       handleMenuItem, "Input-2");
    spa.addMenuItem("InputPage", "InputTest", "Exit",       handleMenuItem, "Input-3" );
}

void setupFSmanagerPage()
{
  const char *fsManagerPage = R"HTML(
    <div id="fsm_fileList" style="display: block;">
    </div>
    <div id="fsm_spaceInfo" class="FSM_space-info" style="display: block;">
      <!-- Space information will be displayed here -->
    </div>    
  )HTML";
  
  spa.addPage("FSmanagerPage", fsManagerPage);

  const char *popupUploadFile = R"HTML(
    <div id="popUpUploadFile">Upload File</div>
    <div id="fsm_fileUpload">
      <input type="file" id="fsm_fileInput">
      <div id="selectedFileName" style="margin-top: 5px; font-style: italic;"></div>
    </div>
    <div style="margin-top: 10px;">
      <button type="button" onClick="closePopup('popup_FS_Manager_Upload_File')">Cancel</button>
      <button type="button" id="uploadButton" onClick="uploadSelectedFile()" disabled>Upload File</button>
    </div>
  )HTML";
  
  const char *popupNewFolder = R"HTML(
    <div id="popupCreateFolder">Create Folder</div>
    <label for="folderNameInput">Folder Name:</label>
    <input type="text" id="folderNameInput" placeholder="Enter folder name">
    <br>
    <button type="button" onClick="closePopup('popup_FS_Manager_New_Folder')">Cancel</button>
    <button type="button" onClick="createFolderFromInput()">Create Folder</button>
  )HTML";

  spa.setPageTitle("FSmanagerPage", "FileSystem Manager");
  //-- Add InputPage menu
  spa.addMenu("FSmanagerPage", "FS Manager");
  spa.addMenuItem("FSmanagerPage", "FS Manager", "List LittleFS", handleMenuItem, "FSM-1");
  spa.addMenuItemPopup("FSmanagerPage", "FS Manager", "Upload File", popupUploadFile);
  spa.addMenuItemPopup("FSmanagerPage", "FS Manager", "Create Folder", popupNewFolder);
  spa.addMenuItem("FSmanagerPage", "FS Manager", "Exit", handleFSexitCallback);

  //--- generate some errors - just for testing
  spa.addMenuItem("notExistingPage", "FS Manager", "Error 1", handleMenuItem, "FSM-2");
  spa.addMenuItem("FSmanagerPage", "XYZ", "Error 2", handleMenuItem, "FSM-2");
  spa.activatePage("notExistingPage");

}


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
}

void listFiles(const char * dirname, int numTabs) {
  // Ensure that dirname starts with '/'
  String path = String(dirname);
  if (!path.startsWith("/")) {
    path = "/" + path;  // Ensure it starts with '/'
  }
  
  File root = LittleFS.open(path);
  
  if (!root) {
    Serial.print("Failed to open directory: ");
    Serial.println(path);
    return;
  }
  
  if (root.isDirectory()) {
    File file = root.openNextFile();
    while (file) {
      for (int i = 0; i < numTabs; i++) {
        Serial.print("\t");
      }
      Serial.print(file.name());
      if (file.isDirectory()) {
        Serial.println("/");
        listFiles(file.name(), numTabs + 1);
      } else {
        Serial.print("\t\t\t");
        Serial.println(file.size());
      }
      file = root.openNextFile();
    }
  }
}
void setup()
{
    Serial.begin(115200);
    delay(3000);

    //-- Connect to WiFi
    network = new Networking();
    
    //-- Parameters: hostname, resetWiFi pin, serial object, baud rate
    debug = network->begin("networkDM", 0, Serial, 115200);
    
    debug->println("\nWiFi connected");
    debug->print("IP address: ");
    debug->println(WiFi.localIP());
    
    spa.begin("/SYS", debug);
    debug->printf("SPAmanager files are located [%s]\n", spa.getSystemFilePath().c_str());
    fsManager.begin();
    fsManager.addSystemFile("/favicon.ico");
    fsManager.addSystemFile(spa.getSystemFilePath() + "/SPAmanager.html", false);
    fsManager.addSystemFile(spa.getSystemFilePath() + "/SPAmanager.css", false);
    fsManager.addSystemFile(spa.getSystemFilePath() + "/SPAmanager.js", false);
    fsManager.addSystemFile(spa.getSystemFilePath() + "/disconnected.html", false);
   
    spa.pageIsLoaded(pageIsLoadedCallback);

    fsManager.setSystemFilePath("/FSM");
    debug->printf("FSmanager files are located [%s]\n", fsManager.getSystemFilePath().c_str());
    spa.includeJsFile(fsManager.getSystemFilePath() + "/FSmanager.js");
    fsManager.addSystemFile("/FSM/FSmanager.js", false);
    spa.includeCssFile(fsManager.getSystemFilePath() + "/FSmanager.css");
    fsManager.addSystemFile(fsManager.getSystemFilePath() + "/FSmanager.css", false);

    setupMainPage();
    setupCounterPage();
    setupInputPage();
    setupFSmanagerPage();
    spa.activatePage("Main");

    if (!LittleFS.begin()) {
      Serial.println("LittleFS Mount Failed");
      return;
    }
    listFiles("/", 0);

    Serial.println("Done with setup() ..\n");

}

void loop()
{
  network->loop();
  spa.server.handleClient();
  spa.ws.loop();
  updateCounter();

  static uint32_t testTimer = 0;
  if (millis() - testTimer > 30000) 
  {
    testTimer = millis();
    debug->printf("setup(): call java function with param: [%s]\n", "Hello World");
    spa.callJsFunction("javaFunctionWithParams", "Hello World");
    debug->printf("setup(): call java function without param\n");
    spa.callJsFunction("javaFunctionWithoutParams");
  }
}
