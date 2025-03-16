
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

    void setup() {
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

    void loop() {
        spamngr.handleClient();
    }
    ```

### Detailed Example

Here is a more detailed example demonstrating how to use the `SPAmanager` library to create a dynamic web page that updates with sensor data:

```cpp
#include <SPAmanager.h>
#include <DHT.h>

#define DHTPIN 2     // Digital pin connected to the DHT sensor
#define DHTTYPE DHT11   // DHT 11

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
// Create a SPAmanager instance with default port (80)
SPAmanager spamngr;

// Or specify a custom port
SPAmanager display(8080);
```

#### `begin(Stream* debugOut = nullptr)`

Initializes the `SPAmanager`. This method should be called in the `setup()` function.
- `debugOut`: Optional parameter for debug output stream.

Example:
```cpp
void setup() {
  Serial.begin(115200);
  
  // Initialize with debug output to Serial
  spamngr.begin(&Serial);
  
  // Or initialize without debug output
  // spamngr.begin();
}
```

#### `addPage(const char* pageName, const char* html)`

Adds a web page to the display manager.
- `pageName`: The URL path where the page will be accessible.
- `html`: The HTML content of the page.

Example:
```cpp
spamngr.addPage("home", R"(
  <html>
    <head><title>Home Page</title></head>
    <body>
      <h1>Welcome to my ESP32 Web Server</h1>
      <p>Current temperature: {{TEMP}} °C</p>
    </body>
  </html>
)");
```

#### `getPlaceholder(const char* pageName, const char* placeholder)`

Gets the value of a placeholder in the HTML content of a web page.
- `pageName`: The URL path of the page containing the placeholder.
- `placeholder`: The placeholder ID to get from the HTML content.

Example:
```cpp
// Get the current value of the TEMP placeholder
PlaceholderValue temp = spamngr.getPlaceholder("home", "TEMP");

// Use the value in different formats
int tempInt = temp.asInt();
float tempFloat = temp.asFloat();
const char* tempStr = temp.c_str();

Serial.print("Current temperature: ");
Serial.println(tempStr);
```

#### `activatePage(const char* pageName)`

Activates a web page, making it the current page being displayed.
- `pageName`: The URL path of the page to activate.

Example:
```cpp
// Switch to the settings page
spamngr.activatePage("settings");
```

#### `setPlaceholder(const char* pageName, const char* placeholder, T value)`

Sets a placeholder value in the HTML content of a web page.

- `pageName`: The URL path of the page containing the placeholder.
- `placeholder`: The placeholder ID to replace in the HTML content.
- `value`: The value to set for the placeholder. This can be of any type that supports conversion to a string.

Example:
```cpp
// Set temperature placeholder with a float value
float temperature = 23.5;
spamngr.setPlaceholder("home", "TEMP", temperature);

// Set humidity placeholder with an integer value
int humidity = 45;
spamngr.setPlaceholder("home", "HUM", humidity);

// Set status placeholder with a string value
spamngr.setPlaceholder("home", "STATUS", "Online");
```

#### `addMenu(const char* pageName, const char* menuName)`

Adds a menu to a specific web page.
- `pageName`: The URL path of the page to add the menu to.
- `menuName`: The name of the menu.

Example:
```cpp
// Add a main menu to the home page
spamngr.addMenu("home", "mainMenu");

// Add a settings menu to the settings page
spamngr.addMenu("settings", "settingsMenu");
```

#### `addMenuItem(const char* pageName, const char* menuName, const char* itemName, std::function<void()> callback)`

Adds a menu item with a callback function to a specific menu on a web page.
- `pageName`: The URL path of the page containing the menu.
- `menuName`: The name of the menu.
- `itemName`: The name of the menu item.
- `callback`: The function to call when the menu item is selected.

Example:
```cpp
// Define a callback function
void turnOnLED() {
  digitalWrite(LED_PIN, HIGH);
  Serial.println("LED turned ON");
}

// Add a menu item that calls the function when clicked
spamngr.addMenuItem("home", "mainMenu", "Turn ON LED", turnOnLED);
```

#### `addMenuItem(const char* pageName, const char* menuName, const char* itemName, const char* url)`

Adds a menu item with a URL to a specific menu on a web page.
- `pageName`: The URL path of the page containing the menu.
- `menuName`: The name of the menu.
- `itemName`: The name of the menu item.
- `url`: The URL to navigate to when the menu item is selected.

Example:
```cpp
// Add a menu item that navigates to the settings page
spamngr.addMenuItem("home", "mainMenu", "Settings", "settings");

// Add a menu item that links to an external website
spamngr.addMenuItem("home", "mainMenu", "Documentation", "https://example.com/docs");
```

#### `addMenuItem(const char* pageName, const char* menuName, const char* itemName, std::function<void(uint8_t)> callback, uint8_t param)`

Adds a menu item with a parameterized callback function to a specific menu on a web page.
- `pageName`: The URL path of the page containing the menu.
- `menuName`: The name of the menu.
- `itemName`: The name of the menu item.
- `callback`: The function to call when the menu item is selected, receiving a uint8_t parameter.
- `param`: The uint8_t value to pass to the callback function when invoked.

Example:
```cpp
void handleMenuAction(uint8_t action) {
    switch (action) {
        case 1:
            // Handle action 1
            digitalWrite(LED_PIN, HIGH);
            break;
        case 2:
            // Handle action 2
            digitalWrite(LED_PIN, LOW);
            break;
    }
}

// In setup or where menus are configured:
spamngr.addMenuItem("home", "mainMenu", "Turn ON LED", handleMenuAction, 1);
spamngr.addMenuItem("home", "mainMenu", "Turn OFF LED", handleMenuAction, 2);
```

#### `addMenuItemPopup(const char* pageName, const char* menuName, const char* menuItem, const char* popupMenu, std::function<void(const std::map<std::string, std::string>&)> callback = nullptr)`

Adds a menu item that opens a popup menu when selected.
- `pageName`: The URL path of the page containing the menu.
- `menuName`: The name of the menu.
- `menuItem`: The name of the menu item that will open the popup.
- `popupMenu`: The name of the popup menu to spamngr.
- `callback`: Optional callback function that receives a map of input values from the popup.

Example:
```cpp
// Define a callback function to handle form submission
void handleWifiSettings(const std::map<std::string, std::string>& values) {
  // Get values from the map
  std::string ssid = values.at("ssid");
  std::string password = values.at("password");
  
  // Use the values to connect to WiFi
  WiFi.begin(ssid.c_str(), password.c_str());
  
  // Show a message
  spamngr.setMessage("Connecting to WiFi...", 3000);
}

// Add a menu item that opens a popup for WiFi settings
spamngr.addMenuItemPopup("settings", "settingsMenu", "WiFi Settings", "wifiPopup", handleWifiSettings);

// The popup HTML would be defined in your page with input fields
// <div id="wifiPopup" class="popup">
//   <input name="ssid" placeholder="WiFi SSID">
//   <input name="password" type="password" placeholder="WiFi Password">
//   <button type="submit">Connect</button>
// </div>
```

#### `disableMenuItem(const char* pageName, const char* menuName, const char* itemName)`

Disables a specific menu item on a web page.
- `pageName`: The URL path of the page containing the menu.
- `menuName`: The name of the menu.
- `itemName`: The name of the menu item to disable.

Example:
```cpp
// Disable the "Reset Device" menu item
spamngr.disableMenuItem("settings", "settingsMenu", "Reset Device");

// You might disable items based on conditions
if (!isWifiConnected) {
  spamngr.disableMenuItem("home", "mainMenu", "Cloud Upload");
}
```

#### `enableMenuItem(const char* pageName, const char* menuName, const char* itemName)`

Enables a specific menu item on a web page.
- `pageName`: The URL path of the page containing the menu.
- `menuName`: The name of the menu.
- `itemName`: The name of the menu item to enable.

Example:
```cpp
// Enable the "Reset Device" menu item
spamngr.enableMenuItem("settings", "settingsMenu", "Reset Device");

// You might enable items based on conditions
if (isWifiConnected) {
  spamngr.enableMenuItem("home", "mainMenu", "Cloud Upload");
}
```

#### `setPageTitle(const char* pageName, const char* title)`

Sets the title of a specific web page.
- `pageName`: The URL path of the page.
- `title`: The new title for the page.

Example:
```cpp
// Set the title of the home page
spamngr.setPageTitle("home", "Smart Home Dashboard");

// Update title based on sensor status
if (alarmTriggered) {
  spamngr.setPageTitle("home", "ALERT: Motion Detected!");
}
```

#### `setMessage(const char* message, int duration)`

Sets a message to be displayed for a specified duration.
- `message`: The message to spamngr.
- `duration`: The duration in milliseconds to display the message.

Example:
```cpp
// Show a temporary message
spamngr.setMessage("Settings saved successfully!", 3000);

// Show a message when a sensor is triggered
if (motionDetected) {
  spamngr.setMessage("Motion detected in living room", 5000);
}
```

#### `setErrorMessage(const char* message, int duration)`

Sets an error message to be displayed for a specified duration.
- `message`: The error message to spamngr.
- `duration`: The duration in milliseconds to display the error message.

Example:
```cpp
// Show an error message
spamngr.setErrorMessage("Failed to connect to WiFi!", 5000);

// Show an error when a sensor reading fails
if (isnan(temperature)) {
  spamngr.setErrorMessage("Temperature sensor error", 3000);
}
```

#### `enableID(const char* pageName, const char* id)`

Enables an HTML element with the specified ID on a web page.
- `pageName`: The URL path of the page containing the element.
- `id`: The ID of the HTML element to enable.

Example:
```cpp
// Enable a button when WiFi is connected
if (WiFi.status() == WL_CONNECTED) {
  spamngr.enableID("home", "connectButton");
}

// Enable a form when the device is ready
spamngr.enableID("settings", "wifiForm");
```

#### `disableID(const char* pageName, const char* id)`

Disables an HTML element with the specified ID on a web page.
- `pageName`: The URL path of the page containing the element.
- `id`: The ID of the HTML element to disable.

Example:
```cpp
// Disable a button when WiFi is disconnected
if (WiFi.status() != WL_CONNECTED) {
  spamngr.disableID("home", "uploadButton");
}

// Disable a form during processing
spamngr.disableID("settings", "wifiForm");
```

#### `includeJsScript(const char* scriptFile)`

Includes a JavaScript file in the web page.

- `scriptFile`: The path to the JavaScript file to include.

This method sends a WebSocket message to the browser to load a JavaScript file. The file must be stored in the LittleFS filesystem. The `includeJsScript()` method must be inside the `pageIsLoaded(callBack)` function

Example:
```cpp
void onPageLoaded() {
  // Include JavaScript files
  spamngr.includeJsScript("/scripts/charts.js");
  spamngr.includeJsScript("/scripts/sensors.js");
  
  // Initialize the page
  spamngr.callJsFunction("initDashboard");
}

void setup() {
  // Set up the display manager
  spamngr.begin();
  
  // Register the page loaded callback
  spamngr.pageIsLoaded(onPageLoaded);
  
  // Add pages and other setup
  // ...
}
```

#### `callJsFunction(const char* functionName)`

Calls a JavaScript function in the browser by its name.

- `functionName`: The name of the JavaScript function to call.

This method sends a WebSocket message to the browser to execute a JavaScript function. The function must be defined in the global scope (window object) in the browser.

Example:
```cpp
// Define a JavaScript function in a .js file (e.g., dashboard.js)
// window.updateChart = function(value) {
//   // Update chart with new value
//   myChart.data.datasets[0].data.push(value);
//   myChart.update();
// }

// Include the script file first
spamngr.includeJsScript("/dashboard.js");

// Then call the JavaScript function with parameters
float sensorValue = readSensor();
char jsCommand[64];
sprintf(jsCommand, "updateChart(%f)", sensorValue);
spamngr.callJsFunction(jsCommand);
```

#### `pageIsLoaded(std::function<void()> callback)`

Sets a callback function to be called when the web page is fully loaded.

- `callback`: The function to call when the page is loaded.

This method is useful for initializing the page with JavaScript files or performing other setup operations after the page has loaded.

Example:
```cpp
void pageLoadedCallback() {
  // This function will be called when the page is loaded
  spamngr.setMessage("Page loaded successfully!", 3000);
  
  // Include JavaScript files
  spamngr.includeJsScript("/charts.js");
  spamngr.includeJsScript("/sensors.js");
  
  // Initialize the page
  spamngr.callJsFunction("initializeDashboard");
  
  // Update placeholders with current values
  spamngr.setPlaceholder("/dashboard", "TEMP", currentTemperature);
  spamngr.setPlaceholder("/dashboard", "HUM", currentHumidity);
}

void setup() {
  // Set up the display manager
  spamngr.begin();
  
  // Register the page loaded callback
  spamngr.pageIsLoaded(pageLoadedCallback);
  
  // Add pages and other setup
  // ...
}
```

#### `handleClient()`

Handles incoming client requests. This method should be called repeatedly in the `loop()` function.

Example:
```cpp
void loop() {
  // Handle client requests
  spamngr.handleClient();
  
  // Other loop code
  // ...
}
```

## Contributing

Contributions are welcome! Please open an issue or submit a pull request on GitHub.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Contact

For any inquiries or support, please contact mrWheel at [Willem@Aandewiel.nl](mailto:Willem@Aandewiel.nl).
