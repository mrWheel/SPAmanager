
# DisplayManager Library

The `DisplayManager` library provides a simple and efficient way to manage Single Page dynamic web pages in Arduino and PlatformIO projects. It abstracts the complexities of handling web content, allowing you to focus on your core application logic.

## Features

- Support for singlepage dynamic web page content management
- Easy-to-use API for web operations
- Compatible with Arduino IDE and PlatformIO

## Installation

### Arduino IDE

1. Download the latest release from the [GitHub releases page](https://github.com/mrWheel/displayManager/releases).
2. Open the Arduino IDE.
3. Go to `Sketch` > `Include Library` > `Add .ZIP Library`.
4. Select the downloaded ZIP file.
5. The library is now installed and ready to use.

### PlatformIO

1. Open your PlatformIO project.
2. Add the following line to your `platformio.ini` file:
    ```ini
    lib_deps = https://github.com/mrWheel/displayManager
    ```
3. Save the `platformio.ini` file.
4. The library will be installed automatically.

## Usage

1. Include the library in your sketch:
    ```cpp
    #include <DisplayManager.h>
    ```
2. Initialize the display manager and add a web page:
    ```cpp
    DisplayManager display;

    void setup() {
        display.begin();
        display.addPage("/", R"(
            <html>
                <head>
                    <title>DisplayManager Example</title>
                </head>
                <body>
                    <h1>Hello, World!</h1>
                    <p>This is a sample web page managed by DisplayManager.</p>
                </body>
            </html>
        )");
    }

    void loop() {
        display.handleClient();
    }
    ```

### Detailed Example

Here is a more detailed example demonstrating how to use the `DisplayManager` library to create a dynamic web page that updates with sensor data:

```cpp
#include <DisplayManager.h>
#include <DHT.h>

#define DHTPIN 2     // Digital pin connected to the DHT sensor
#define DHTTYPE DHT11   // DHT 11

DHT dht(DHTPIN, DHTTYPE);
DisplayManager display;

void setup()
{
    Serial.begin(115200);
    dht.begin();
    display.begin();
    display.addPage("/", R"(
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

    display.setPlaceholder("/", "temp", String(t));
    display.setPlaceholder("/", "hum", String(h));

    display.handleClient();
}
```

In this example, the web page displays the temperature and humidity readings from a DHT11 sensor. The page automatically refreshes every 5 seconds to show the latest sensor data.

## API Reference

#### `begin(Stream* debugOut = nullptr)`

Initializes the `DisplayManager`. This method should be called in the `setup()` function.
- `debugOut`: Optional parameter for debug output stream.

#### `addPage(const char* pageName, const char* html)`

Adds a web page to the display manager.
- `pageName`: The URL path where the page will be accessible.
- `html`: The HTML content of the page.

#### `activatePage(const char* pageName)`

Activates a web page, making it the current page being displayed.
- `pageName`: The URL path of the page to activate.

#### `setPlaceholder(const char* pageName, const char* placeholder, T value)`

Sets a placeholder value in the HTML content of a web page.

- `pageName`: The URL path of the page containing the placeholder.
- `placeholder`: The placeholder ID to replace in the HTML content.
- `value`: The value to set for the placeholder. This can be of any type that supports conversion to a string.

#### `addMenu(const char* pageName, const char* menuName)`

Adds a menu to a specific web page.
- `pageName`: The URL path of the page to add the menu to.
- `menuName`: The name of the menu.

#### `addMenuItem(const char* pageName, const char* menuName, const char* itemName, std::function<void()> callback)`

Adds a menu item with a callback function to a specific menu on a web page.
- `pageName`: The URL path of the page containing the menu.
- `menuName`: The name of the menu.
- `itemName`: The name of the menu item.
- `callback`: The function to call when the menu item is selected.

#### `addMenuItem(const char* pageName, const char* menuName, const char* itemName, const char* url)`

Adds a menu item with a URL to a specific menu on a web page.
- `pageName`: The URL path of the page containing the menu.
- `menuName`: The name of the menu.
- `itemName`: The name of the menu item.
- `url`: The URL to navigate to when the menu item is selected.

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
            break;
        case 2:
            // Handle action 2
            break;
    }
}

// In setup or where menus are configured:
display.addMenuItem("page1", "mainMenu", "Action1", handleMenuAction, 1);
display.addMenuItem("page1", "mainMenu", "Action2", handleMenuAction, 2);
```

#### `disableMenuItem(const char* pageName, const char* menuName, const char* itemName)`

Disables a specific menu item on a web page.
- `pageName`: The URL path of the page containing the menu.
- `menuName`: The name of the menu.
- `itemName`: The name of the menu item to disable.

#### `enableMenuItem(const char* pageName, const char* menuName, const char* itemName)`

Enables a specific menu item on a web page.
- `pageName`: The URL path of the page containing the menu.
- `menuName`: The name of the menu.
- `itemName`: The name of the menu item to enable.

#### `callJsFunction(const char* functionName)`

Calls a JavaScript function in the browser by its name.

- `functionName`: The name of the JavaScript function to call.

This method sends a WebSocket message to the browser to execute a JavaScript function. The function must be defined in the global scope (window object) in the browser.

Example:
```cpp
// Define a JavaScript function in a .js file (e.g., test.js)
// window.logSomeMessages = function() {
//   console.log('This is a log message');
// }

// Include the script file first
display.includeJsScript("/test.js");

// Then call the JavaScript function
display.callJsFunction("logSomeMessages");
```

#### `includeJsScript(const char* scriptFile)`

Includes a JavaScript file in the web page.

- `scriptFile`: The path to the JavaScript file to include.

This method sends a WebSocket message to the browser to load a JavaScript file. The file must be stored in the LittleFS filesystem. The `includeJsScript()` method must be inside the `pageIsLoaded(callBack)` function

Example:
```cpp
void callBack()
{
  // Include a JavaScript file
  display.includeJsScript("/myScript.js");

  // You can include multiple script files
  display.includeJsScript("/utilities.js");
  display.includeJsScript("/animations.js");
}
```

#### `pageIsLoaded(std::function<void()> callback)`

Sets a callback function to be called when the web page is fully loaded.

- `callback`: The function to call when the page is loaded.

This method is useful for initializing the page with JavaScript files or performing other setup operations after the page has loaded.

Example:
```cpp
void pageLoadedCallback() {
  // This function will be called when the page is loaded
  display.setMessage("Page loaded successfully!", 5);
  
  // Include JavaScript files
  display.includeJsScript("/myScript.js");
  
  // Call JavaScript functions
  display.callJsFunction("initializePage");
}

void setup() {
  // Set up the display manager
  display.begin();
  
  // Register the page loaded callback
  display.pageIsLoaded(pageLoadedCallback);
  
  // Add pages and other setup
  // ...
}
```

#### `setMessage(const char* message, int duration)`

Sets a message to be displayed for a specified duration.
- `message`: The message to display.
- `duration`: The duration in milliseconds to display the message.

#### `setErrorMessage(const char* message, int duration)`

Sets an error message to be displayed for a specified duration.
- `message`: The error message to display.
- `duration`: The duration in milliseconds to display the error message.

#### `handleClient()`

Handles incoming client requests. This method should be called repeatedly in the `loop()` function.

## Contributing

Contributions are welcome! Please open an issue or submit a pull request on GitHub.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Contact

For any inquiries or support, please contact mrWheel at [Willem@Aandewiel.nl](mailto:Willem@Aandewiel.nl).
