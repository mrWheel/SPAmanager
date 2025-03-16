
# SPAmanager Library

The `SPAmanager` library provides a simple and efficient way to manage Single Page dynamic web pages in Arduino and PlatformIO projects. It abstracts the complexities of handling web content, allowing you to focus on your core application logic.

## Features

- Support for singlepage dynamic web page content management
- Easy-to-use API for web operations
- Compatible with Arduino IDE and PlatformIO

## Installation

### Arduino IDE

1. Download the latest release from the [GitHub](https://github.com/mrWheel/SPAmanager).
2. Open the Arduino IDE.
3. Go to `Sketch` > `Include Library` > `Add .ZIP Library`.
4. Select the downloaded ZIP file.
5. The library is now installed and ready to use.

### PlatformIO

1. Open your PlatformIO project.
2. Add the following line to your `platformio.ini` file:
    ```ini
    lib_deps = https://github.com/mrWheel/SPAmanager
    ```
3. Save the `platformio.ini` file.
4. The library will be installed automatically.

## Usage

1. Include the library in your sketch:
    ```cpp
    #include <SPAmanager.h>
    ```
2. Initialize the display manager and add a web page:
    ```cpp
    SPAmanager spamngr;

    void setup() 
    {
        spamngr.begin();
        spamngr.addPage("Main", R"(
            <html>
                <head>
                    <title>SPAmanager Example</title>
                </head>
                <body>
                    <h1>Hello, World!</h1>
                    <p>This is a sample web page managed by SPAmanager.</p>
                </body>
            </html>
        )");
    }

    void loop() 
    {
        spamngr.handleClient();
    }
    ```

### Detailed Example

Here is a more detailed example demonstrating how to use the `SPAmanager` library to create a dynamic web page that updates with sensor data:

```cpp
#include <SPAmanager.h>
#include <DHT.h>

#define DHTPIN 2        //-- Digital pin connected to the DHT sensor
#define DHTTYPE DHT11   //-- DHT 11

DHT dht(DHTPIN, DHTTYPE);
SPAmanager spamngr;

void setup()
{
    Serial.begin(115200);
    dht.begin();
    spamngr.begin();
    spamngr.addPage("Main", R"(
        <html>
            <head>
                <title>Sensor Data</title>
                <meta http-equiv="refresh" content="5">
            </head>
            <body>
                <h1>Sensor Data</h1>
                <p id="temp">Temperature: {{TEMP}} &deg;C</p>
                <p id="hum">Humidity: {{HUM}} %</p>
            </body>
        </html>
    )");
}

void loop()
{
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (isnan(h) || isnan(t))
    {
        Serial.println("Failed to read from DHT sensor!");
        return;
    }

    spamngr.setPlaceholder("Main", "temp", String(t));
    spamngr.setPlaceholder("Main", "hum", String(h));

    spamngr.handleClient();
}
```

In this example, the web page displays the temperature and humidity readings from a DHT11 sensor. The page automatically refreshes every 5 seconds to show the latest sensor data.

## API Reference

#### `SPAmanager(uint16_t port = 80)`

Constructor for the SPAmanager class.
- `port`: The port number for the web server (default is 80).

Example:
```cpp
//-- Create a SPAmanager instance with default port (80)
SPAmanager spaManager;

//-- Or specify a custom port
SPAmanager spaManager(8080);
```

#### `begin(const char* systemPath, Stream* debugOut = nullptr)`

Initializes the SPAmanager with system files path and optional debug output.
- `systemPath`: Path to the system files (HTML, CSS, JS)
- `debugOut`: Optional parameter for debug output stream.

Example:
```cpp
void setup() 
{
  Serial.begin(115200);
  
  //-- Initialize with system files in /SYS directory and debug output to Serial
  spaManager.begin("/SYS", &Serial);
  
  //-- Or initialize without debug output
  //-- spaManager.begin("/SYS");
}
```

#### `addPage(const char* pageName, const char* html)`

Adds a web page to the display manager.
- `pageName`: The name of the page.
- `html`: The HTML content of the page.

Example:
```cpp
//-- Add a simple home page
spaManager.addPage("Home", R"(
  <div>
    <h1>Welcome to my ESP32 Web Server</h1>
    <p>Current temperature: <span id="temp">--</span> °C</p>
    <button id="refreshBtn">Refresh</button>
  </div>
)");
```

#### `activatePage(const char* pageName)`

Activates a web page, making it the current page being displayed.
- `pageName`: The name of the page to activate.

Example:
```cpp
//-- Switch to the settings page
spaManager.activatePage("Settings");
```

#### `setPageTitle(const char* pageName, const char* title)`

Sets the title of a specific web page.
- `pageName`: The name of the page.
- `title`: The new title for the page.

Example:
```cpp
//-- Set the title of the home page
spaManager.setPageTitle("Home", "Smart Home Dashboard");

//-- Update title based on sensor status
if (alarmTriggered) 
{
  spaManager.setPageTitle("Home", "ALERT: Motion Detected!");
}
```

#### `addMenu(const char* pageName, const char* menuName)`

Adds a menu to a specific web page.
- `pageName`: The name of the page to add the menu to.
- `menuName`: The name of the menu.

Example:
```cpp
//-- Add a main menu to the home page
spaManager.addMenu("Home", "Main Menu");

//-- Add a settings menu to the settings page
spaManager.addMenu("Settings", "Settings Menu");
```

#### `addMenuItem(const char* pageName, const char* menuName, const char* itemName, std::function<void()> callback)`

Adds a menu item with a callback function to a specific menu on a web page.
- `pageName`: The name of the page containing the menu.
- `menuName`: The name of the menu.
- `itemName`: The name of the menu item.
- `callback`: The function to call when the menu item is selected.

Example:
```cpp
//-- Define a callback function
void turnOnLED() 
{
  digitalWrite(LED_PIN, HIGH);
  Serial.println("LED turned ON");
}

//-- Add a menu item that calls the function when clicked
spaManager.addMenuItem("Home", "Main Menu", "Turn ON LED", turnOnLED);
```

#### `addMenuItem(const char* pageName, const char* menuName, const char* itemName, const char* url)`

Adds a menu item with a URL to a specific menu on a web page.
- `pageName`: The name of the page containing the menu.
- `menuName`: The name of the menu.
- `itemName`: The name of the menu item.
- `url`: The URL to navigate to when the menu item is selected.

Example:
```cpp
//-- Add a menu item that navigates to the settings page
spaManager.addMenuItem("Home", "Main Menu", "Settings", "/settings");

//-- Add a menu item that links to an external website
spaManager.addMenuItem("Home", "Main Menu", "Documentation", "https://example.com/docs");
```

#### `addMenuItem(const char* pageName, const char* menuName, const char* itemName, std::function<void(const char*)> callback, const char* param)`

Adds a menu item with a parameterized callback function to a specific menu on a web page.
- `pageName`: The name of the page containing the menu.
- `menuName`: The name of the menu.
- `itemName`: The name of the menu item.
- `callback`: The function to call when the menu item is selected, receiving a const char* parameter.
- `param`: The string value to pass to the callback function when invoked.

Example:
```cpp
void handleMenuAction(const char* action) 
{
  if (strcmp(action, "on") == 0) 
  {
    digitalWrite(LED_PIN, HIGH);
    spaManager.setMessage("LED turned ON", 3000);
  } 
  else if (strcmp(action, "off") == 0) 
  {
    digitalWrite(LED_PIN, LOW);
    spaManager.setMessage("LED turned OFF", 3000);
  }
}

//-- In setup or where menus are configured:
spaManager.addMenuItem("Home", "Main Menu", "Turn ON LED", handleMenuAction, "on");
spaManager.addMenuItem("Home", "Main Menu", "Turn OFF LED", handleMenuAction, "off");
```

#### `addMenuItemPopup(const char* pageName, const char* menuName, const char* menuItem, const char* popupMenu, std::function<void(const std::map<std::string, std::string>&)> callback = nullptr)`

Adds a menu item that opens a popup menu when selected.
- `pageName`: The name of the page containing the menu.
- `menuName`: The name of the menu.
- `menuItem`: The name of the menu item that will open the popup.
- `popupMenu`: The HTML content of the popup menu.
- `callback`: Optional callback function that receives a map of input values from the popup.

Example:
```cpp
//-- Define a callback function to handle form submission
void handleWifiSettings(const std::map<std::string, std::string>& values) {
  //-- Get values from the map
  std::string ssid = values.at("ssid");
  std::string password = values.at("password");
  
  //-- Use the values to connect to WiFi
  WiFi.begin(ssid.c_str(), password.c_str());
  
  //-- Show a message
  spaManager.setMessage("Connecting to WiFi...", 3000);
}

//-- Add a menu item that opens a popup for WiFi settings
const char* wifiPopupHtml = R"(
  <div class="popup-content">
    <h3>WiFi Settings</h3>
    <input name="ssid" placeholder="WiFi SSID">
    <input name="password" type="password" placeholder="WiFi Password">
    <button type="submit">Connect</button>
  </div>
)";

spaManager.addMenuItemPopup("Settings", "Settings Menu", "WiFi Settings", wifiPopupHtml, handleWifiSettings);
```

#### `enableMenuItem(const char* pageName, const char* menuName, const char* itemName)`

Enables a specific menu item on a web page.
- `pageName`: The name of the page containing the menu.
- `menuName`: The name of the menu.
- `itemName`: The name of the menu item to enable.

Example:
```cpp
//-- Enable the "Reset Device" menu item
spaManager.enableMenuItem("Settings", "Settings Menu", "Reset Device");

//-- You might enable items based on conditions
if (isWifiConnected) 
{
  spaManager.enableMenuItem("Home", "Main Menu", "Cloud Upload");
}
```

#### `disableMenuItem(const char* pageName, const char* menuName, const char* itemName)`

Disables a specific menu item on a web page.
- `pageName`: The name of the page containing the menu.
- `menuName`: The name of the menu.
- `itemName`: The name of the menu item to disable.

Example:
```cpp
//-- Disable the "Reset Device" menu item
spaManager.disableMenuItem("Settings", "Settings Menu", "Reset Device");

//-- You might disable items based on conditions
if (!isWifiConnected) 
{
  spaManager.disableMenuItem("Home", "Main Menu", "Cloud Upload");
}
```

#### `setMessage(const char* message, int duration)`

Sets a message to be displayed for a specified duration.
- `message`: The message to display.
- `duration`: The duration in seconds to display the message.

Example:
```cpp
//-- Show a temporary message for 3 seconde
spaManager.setMessage("Settings saved successfully!", 3);

//-- Show a message when a sensor is triggered
if (motionDetected) 
{
  spaManager.setMessage("Motion detected in living room", 5);
}
```

#### `setErrorMessage(const char* message, int duration)`

Sets an error message to be displayed for a specified duration.
- `message`: The error message to display.
- `duration`: The duration in seconds to display the error message.

Example:
```cpp
//-- Show an error message for 5 seconds
spaManager.setErrorMessage("Failed to connect to WiFi!", 5);

//-- Show an error when a sensor reading fails
if (isnan(temperature)) 
{
  spaManager.setErrorMessage("Temperature sensor error", 3);
}
```

#### `callJsFunction(const char* functionName)`

Calls a JavaScript function in the browser by its name.
- `functionName`: The name of the JavaScript function to call.

Example:
```cpp
//-- Call a simple JavaScript function
spaManager.callJsFunction("refreshData");

// Call a JavaScript function with parameters
char jsCommand[64];
float sensorValue = 23.5;
sprintf(jsCommand, "updateChart(%f)", sensorValue);
spaManager.callJsFunction(jsCommand);
```

#### `setPlaceholder(const char* pageName, const char* placeholder, T value)`

Sets a placeholder value in the HTML content of a web page.
- `pageName`: The name of the page containing the placeholder.
- `placeholder`: The ID of the element to update.
- `value`: The value to set (can be int, float, double, or const char*).

Example:
```cpp
//-- Set temperature placeholder with a float value
float temperature = 23.5;
spaManager.setPlaceholder("Home", "temp", temperature);

//-- Set humidity placeholder with an integer value
int humidity = 45;
spaManager.setPlaceholder("Home", "humidity", humidity);

//-- Set status placeholder with a string value
spaManager.setPlaceholder("Home", "status", "Online");
```

#### `getPlaceholder(const char* pageName, const char* placeholder)`

Gets the value of a placeholder in the HTML content of a web page.
- `pageName`: The name of the page containing the placeholder.
- `placeholder`: The ID of the element to get the value from.

Example:
```cpp
//-- Get the current value of the temperature placeholder
PlaceholderValue temp = spaManager.getPlaceholder("Home", "temp");

//-- Use the value in different formats
int tempInt = temp.asInt();
float tempFloat = temp.asFloat();
const char* tempStr = temp.c_str();

Serial.print("Current temperature: ");
Serial.println(tempStr);
```

#### `enableID(const char* pageName, const char* id)`

Enables an HTML element with the specified ID on a web page by setting its display style to "block".
- `pageName`: The name of the page containing the element.
- `id`: The ID of the HTML element to enable.

Example:
```cpp
//-- Enable a button when WiFi is not connected
if (WiFi.status() == WL_CONNECTED) 
{
  spaManager.disableID("Home", "connectButton");
}
else
{
  spaManager.enableID("Home", "connectButton");
}

//-- Enable a form when the device is ready
spaManager.enableID("Settings", "wifiForm");
```

#### `disableID(const char* pageName, const char* id)`

Disables an HTML element with the specified ID on a web page by setting its display style to "none".
- `pageName`: The name of the page containing the element.
- `id`: The ID of the HTML element to disable.

Example:
```cpp
//-- Disable a button when WiFi is disconnected
if (WiFi.status() != WL_CONNECTED) 
{
  spaManager.disableID("Home", "uploadButton");
}
else
{
  spaManager.enableID("Home", "uploadButton");
}

//-- Disable a form during processing
spaManager.disableID("Settings", "wifiForm");
```

#### `pageIsLoaded(std::function<void()> callback)`

Sets a callback function to be called when the web page is fully loaded.
- `callback`: The function to call when the page is loaded.

Example:
```cpp
void onPageLoaded() 
{
  //-- This function will be called when the page is loaded
  spaManager.setMessage("Page loaded successfully!", 3);
  
  //-- Initialize the page
  spaManager.callJsFunction("initializeDashboard");
  
  //-- Update placeholders with current values
  spaManager.setPlaceholder("Dashboard", "temp", currentTemperature);
  spaManager.setPlaceholder("Dashboard", "humidity", currentHumidity);
}

void setup() 
{
  //-- Set up the display manager
  spaManager.begin("/SYS");
  
  //-- Register the page loaded callback
  spaManager.pageIsLoaded(onPageLoaded);
  
  //-- Include JavaScript and CSS files
  spaManager.includeJsFile("/scripts/charts.js");
  spaManager.includeCssFile("/styles/custom.css");
  
  //-- Add pages and other setup
  // ...
}
```

#### `getSystemFilePath() const`

Returns the path to the system files directory.

Example:
```cpp
//-- Get the system files path
std::string sysPath = spaManager.getSystemFilePath();

//-- Use the path to construct file paths
std::string cssPath = sysPath + "/custom.css";
Serial.println(("CSS file path: " + cssPath).c_str());
```

#### `includeCssFile(const std::string &cssFile)`

Includes a CSS file in the web page.
- `cssFile`: The path to the CSS file to include.

Example:
```cpp
void onPageLoaded() 
{
  //-- initialize fields or what not
}

void setup() 
{
  //-- Set up the display manager
  spaManager.begin("/SYS");
  
  //-- Register the page loaded callback
  spaManager.pageIsLoaded(onPageLoaded);

  //-- Include CSS files
  spaManager.includeCssFile("/styles/main.css");
  spaManager.includeCssFile("/styles/dashboard.css");

  //-- Conditionally include theme CSS
  if (darkModeEnabled) 
  {
    spaManager.includeCssFile("/styles/dark-theme.css");
  } 
  else 
  {
    spaManager.includeCssFile("/styles/light-theme.css");
  }  

}
```

#### `includeJsFile(const std::string &scriptFile)`

Includes a JavaScript file in the web page.
- `scriptFile`: The path to the JavaScript file to include.

Example:
```cpp
void onPageLoaded() 
{
  //-- Initialize the dashboard after including scripts
  spaManager.callJsFunction("initDashboard");
}

void setup() 
{
  // Set up the display manager
  spaManager.begin("/SYS");
  
  // Register the page loaded callback
  spaManager.pageIsLoaded(onPageLoaded);

  //-- Include JavaScript files
  //-- these files are loaded by the SPAmanager as
  //-- soon as the html page is fully loaded
  spaManager.includeJsFile("/scripts/utils.js");
  spaManager.includeJsFile("/scripts/charts.js");
  spaManager.includeJsFile("/scripts/sensors.js");  

}
