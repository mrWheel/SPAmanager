//----- SPAmanager.cpp -----
#include "SPAmanager.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <sstream>
#include <WString.h>

const char* SPAmanager::PAGES_DIRECTORY = "/SPApages/";

const char* SPAmanager::DEFAULT_ERROR_PAGE = R"HTML(
  <div style="text-align: center; padding: 20px;">
    <h2>Error: pages directory not found on the LittleFS</h2>
    <p>The required directory for storing pages could not be found.</p>
    <p>Please upload the FileSystem image and make sure there is enough free space.</p>
  </div>
)HTML";

const char* SPAmanager::MINIMAL_FSMANAGER_PAGE = R"HTML(
  <div style="text-align: center; padding: 20px;">
    <h2>File System Manager (Limited Mode)</h2>
    <p>Running in limited functionality mode due to memory constraints.</p>
    <div id="fileList"></div>
  </div>
)HTML";

SPAmanager::SPAmanager(uint16_t port) 
    : server(port)
    , ws(81)
    , debugOut(nullptr)
    , currentClient(0)
    , hasConnectedClient(false)
    , pageLoadedCallback(nullptr)
    , currentMessage{0}
    , isError(false)
    , messageEndTime(0)
    , isPopup(false)
    , showCloseButton(false)
    , menus()
    , pages()
    , activePage(nullptr)
    , servedFiles()  // Initialize empty set
{
  debug(("SPAmanager::  constructor called with port: " + std::to_string(port)).c_str());
}



void SPAmanager::begin(const char* systemPath, Stream* debugOut) 
{
    // Convert to std::string for manipulation
    std::string path = systemPath;
    
    // Ensure it starts with '/' but does not end with '/'
    if (!path.empty() && path.front() != '/') {
      path = "/" + path;  // Add '/' at the beginning if not already present
    }

    if (path != "/" && path.back() == '/') {
      path.pop_back();  // Remove trailing '/' if it's not the root '/'
    }

    // Store in rootSystemPath
    rootSystemPath = path;

    this->debugOut = debugOut;

    debug(("SPAmanager::begin: begin(): called with rootSystemPath: [" + rootSystemPath + "]").c_str());
    
    // Initialize the filesystem with robust handling
    //if (!initializeFilesystem()) {
    //    error("Failed to initialize filesystem. Some features may not work correctly.");
    //}
    
    setupWebServer();

} //  begin()

void SPAmanager::setupWebServer() 
{
    debug("setupWebServer() called");
    ws.begin();
    ws.onEvent([this](uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
        handleWebSocketEvent(num, type, payload, length);
    });

    // Serve the pages directory
    std::string pagesDir = PAGES_DIRECTORY;
    if (pagesDir[0] != '/') {
        pagesDir = "/" + pagesDir; // Ensure it starts with a slash
    }
    server.serveStatic(PAGES_DIRECTORY, LittleFS, pagesDir.c_str());
    std::string msg = "server.serveStatic(" + std::string(PAGES_DIRECTORY) + ", LittleFS, " + pagesDir + ")";
    debug(msg.c_str());

    // Serve system files
    std::string sysPath = rootSystemPath;
    if (sysPath[0] != '/') {
        sysPath = "/" + sysPath; // Ensure it starts with a slash
    }
    // Debug the system path
    debug(("System path: [" + sysPath + "]").c_str());
    
    // Check if the system directory exists
    if (!LittleFS.exists(sysPath.c_str())) {
        error(("System directory does not exist: " + sysPath).c_str());
        
        // Try to list all directories in the root to help debug
        File root = LittleFS.open("/", "r");
        if (root && root.isDirectory()) {
            File file = root.openNextFile();
            while (file) {
                debug(("Found in root: " + std::string(file.name())).c_str());
                file = root.openNextFile();
            }
        }
    } else {
        debug(("System directory exists: " + sysPath).c_str());
    }

    // Serve each system file individually
    std::string cssFilePath = sysPath + "/SPAmanager.css";
    std::string htmlFilePath = sysPath + "/SPAmanager.html";
    std::string jsFilePath = sysPath + "/SPAmanager.js";
    std::string disconnectedFilePath = sysPath + "/disconnected.html";

    // Debug each file path
    debug(("CSS file path: [" + cssFilePath + "]").c_str());
    debug(("HTML file path: [" + htmlFilePath + "]").c_str());
    debug(("JS file path: [" + jsFilePath + "]").c_str());
    debug(("Disconnected file path: [" + disconnectedFilePath + "]").c_str());
    
    // Check if each file exists
    if (!LittleFS.exists(cssFilePath.c_str())) {
        error(("CSS file does not exist: " + cssFilePath).c_str());
    }
    if (!LittleFS.exists(htmlFilePath.c_str())) {
        error(("HTML file does not exist: " + htmlFilePath).c_str());
    }
    if (!LittleFS.exists(jsFilePath.c_str())) {
        error(("JS file does not exist: " + jsFilePath).c_str());
    }
    if (!LittleFS.exists(disconnectedFilePath.c_str())) {
        error(("Disconnected file does not exist: " + disconnectedFilePath).c_str());
    }

    // Serve the system files
    server.serveStatic("/SPAmanager.html", LittleFS, htmlFilePath.c_str());
    debug(("server.serveStatic(/SPAmanager.html, LittleFS, " + htmlFilePath + ")").c_str());

    server.serveStatic("/SPAmanager.css", LittleFS, cssFilePath.c_str());
    debug(("server.serveStatic(/SPAmanager.css, LittleFS, " + cssFilePath + ")").c_str());
    
    server.serveStatic("/SPAmanager.js", LittleFS, jsFilePath.c_str());
    debug(("server.serveStatic(/SPAmanager.js, LittleFS, " + jsFilePath + ")").c_str());
    
    server.serveStatic("/disconnected.html", LittleFS, disconnectedFilePath.c_str());
    debug(("server.serveStatic(/disconnected.html, LittleFS, " + disconnectedFilePath + ")").c_str());

    //server.on("/", HTTP_GET, [this]() {
    //    server.send(200, "text/html", generateHTML().c_str());
    //});
    server.on("/", HTTP_GET, [this]() {
        server.sendHeader("Location", "/SPAmanager.html", true);
        server.send(302, "text/plain", "");
    });
    server.begin();

  } // setupWebServer()


void SPAmanager::setLocalEventHandler(std::function<void(uint8_t, WStype_t, uint8_t*, size_t)> callback)
{
  debug("setLocalEventHandler() called");
  localEventsCallback = callback;
}

void SPAmanager::handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) 
{
    bool eventHandled = false;
    
    if (type == WStype_CONNECTED) 
    {
        debug("WebSocket client connected");
        
        // If we already have a client connected
        if (hasConnectedClient)
        {
            // Send redirect command to previous client
            const size_t capacity = JSON_OBJECT_SIZE(2) + 128;
            DynamicJsonDocument doc(capacity);
            doc["type"] = "redirect";
            doc["url"] = "/disconnected.html";
            
            std::string output;
            serializeJson(doc, output);
            ws.sendTXT(currentClient, output.c_str());
            
            // Force disconnect the previous client
            ws.disconnect(currentClient);
            debug("Redirected and disconnected previous client");
            
            // Small delay to ensure previous client is fully disconnected
            delay(100);
        }
        
        // Store new client info
        currentClient = num;
        hasConnectedClient = true;
        
        // Set the page title for the new client
        if (activePage)
        {
          setHeaderTitle(activePage->title);
        }
        
        broadcastState();
        eventHandled = true;
    }
    else if (type == WStype_DISCONNECTED)
    {
        // Only clear connection if it's our current client
        if (num == currentClient)
        {
            hasConnectedClient = false;
            debug("Current client disconnected");
        }
        eventHandled = true;
    }
    else if (type == WStype_TEXT)
    {
        // Only process messages from current client
        if (!hasConnectedClient || num != currentClient)
        {
            return;
        }

        payload[length] = 0;
        std::string message = std::string((char*)payload);
        //const size_t capacity = JSON_OBJECT_SIZE(10) + 256;
        const size_t capacity = 5000; // Adjusted for larger messages
        DynamicJsonDocument doc(capacity);
        if (doc.capacity() == 0)
        {
            debug("Failed to allocate JSON buffer for WebSocket message");
            return;
        }
        
        DeserializationError jsonError = deserializeJson(doc, message);
        
        if (jsonError)
        {
            debug(("JSON deserialization failed: " + std::string(jsonError.c_str())).c_str());
            debug(("Received message: " + message + " [" + std::to_string(message.length()) + " bytes]").c_str());
            return;
        }

        if (doc["type"] == "menuClick") {
            const char* menuName = doc["menu"];
            const char* itemName = doc["item"];
            
            if (activePage) {
                for (const auto& menu : menus) 
                {
                    if (strcmp(menu.name, menuName) == 0 && strcmp(menu.pageName, activePage->name) == 0) 
                    {
                        for (const auto& item : menu.items) 
                        {
                            if (strcmp(item.name, itemName) == 0 && item.callback && !item.disabled) 
                            {
                                item.callback();
                                break;
                            }
                        }
                        break;
                    }
                }
            }
            eventHandled = true;
        }
        else if (doc["type"] == "inputChange") 
        {
            const char* placeholder = doc["placeholder"];
            const char* value = doc["value"];
            
            if (activePage) 
            {
                for (auto& page : pages) {
                    if (strcmp(page.name, activePage->name) == 0) {
                        if (!page.isFileStorage) {
                            debug("Page is not using file storage, skipping input change");
                            return;
                        }
                        
                        // Read the current content from file
                        std::string content = getPageContent(page);
                        if (content.empty() || content == DEFAULT_ERROR_PAGE) {
                            debug("Failed to read page content or using error page");
                            return;
                        }
                        
                        std::string idStr1 = std::string("id='") + placeholder + "'";
                        std::string idStr2 = std::string("id=\"") + placeholder + "\"";
                        size_t pos = content.find(idStr1);
                        if (pos == std::string::npos) {
                            pos = content.find(idStr2);
                        }
                        
                        if (pos != std::string::npos) {
                            size_t inputStart = content.rfind("<input", pos);
                            if (inputStart != std::string::npos && inputStart < pos) {
                                // Find value attribute or closing bracket
                                size_t valueStart1 = content.find("value='", inputStart);
                                size_t valueStart2 = content.find("value=\"", inputStart);
                                size_t closingBracket = content.find(">", inputStart);
                                
                                if (valueStart1 != std::string::npos && valueStart1 < closingBracket) {
                                    // Value attribute exists with single quotes
                                    size_t valueEnd = content.find("'", valueStart1 + 7);
                                    if (valueEnd != std::string::npos) {
                                        content.replace(valueStart1 + 7, valueEnd - (valueStart1 + 7), value);
                                    }
                                } else if (valueStart2 != std::string::npos && valueStart2 < closingBracket) {
                                    // Value attribute exists with double quotes
                                    size_t valueEnd = content.find("\"", valueStart2 + 7);
                                    if (valueEnd != std::string::npos) {
                                        content.replace(valueStart2 + 7, valueEnd - (valueStart2 + 7), value);
                                    }
                                } else {
                                    // No value attribute, insert before closing bracket
                                    content.insert(closingBracket, " value=\"" + std::string(value) + "\"");
                                }
                                
                                // Write the updated content back to the file
                                File pageFile = LittleFS.open(page.filePath, "w");
                                if (pageFile) {
                                    pageFile.print(content.c_str());
                                    pageFile.close();
                                } else {
                                    this->error(("Failed to open page file for writing: " + std::string(page.filePath)).c_str());
                                }
                            }
                        }
                        break;
                    }
                }
            }
            eventHandled = true;
        }
        else if (doc["type"] == "pageLoaded") 
        {
          debug("WebSocket: pageLoaded message received");
          if (!firstPageName.empty()) 
          {
            debug(("Activating first page: [" + firstPageName + "]").c_str());
            activatePage(firstPageName.c_str());
          }
          
          // Include all scripts in servedFiles
          debug("Including all files in servedFiles");
          for (const auto& scriptPath : servedFiles) 
          {
            debug(("pageLoaded:: Including file: [" + scriptPath + "]").c_str());
            
            // Extract the script file name from the full path
            std::string sanitizedJsFile;
            size_t pos = scriptPath.find(rootSystemPath);
            if (pos != std::string::npos) 
            {
              // If rootSystemPath is found, remove it from the path
              sanitizedJsFile = scriptPath.substr(pos + rootSystemPath.length());
            } else {
              // If rootSystemPath is not found, just use the scriptPath itself
              sanitizedJsFile = scriptPath;
            }
            
            // Ensure the file path starts with a '/'
            if (!sanitizedJsFile.empty() && sanitizedJsFile[0] != '/') 
            {
              sanitizedJsFile = "/" + sanitizedJsFile;
            }
          
            debug(("Including file: [" + sanitizedJsFile + "]").c_str());
          
            const size_t capacity = JSON_OBJECT_SIZE(2);
            DynamicJsonDocument scriptDoc(capacity);
          
            // Determine if this is a CSS file or JS file based on extension
            if (sanitizedJsFile.find(".css") != std::string::npos) 
            {
              scriptDoc["event"] = "includeCssFile";
            } else {
              scriptDoc["event"] = "includeJsFile";
            }
            scriptDoc["data"] = sanitizedJsFile.c_str();
          
            std::string output;
            serializeJson(scriptDoc, output);
          
            if (!output.empty()) 
            {
              debug(("Broadcasting include message: [" + output + "]").c_str());
              ws.broadcastTXT(output.c_str(), output.length());
            }
          }
          if (pageLoadedCallback) 
          {
            pageLoadedCallback();
          }
          eventHandled = true;
        }
        else if (doc["type"] == "jsFunctionResult") 
        {
          const char* functionName = doc["functionName"];
          bool success = doc["success"];
          
          // Call the existing handleJsFunctionResult method
          handleJsFunctionResult(functionName, success);
          eventHandled = true;
        }
        else if (doc["type"] == "process") 
        {
          const char* processType = doc["processType"];
          
          // Check if popupId exists before accessing it
          const char* popupId = "";
          if (doc.containsKey("popupId")) {
            popupId = doc["popupId"];
          }
          
          debug(("WebSocket: process message received with type: " + std::string(processType) + " for popup: " + std::string(popupId)).c_str());
          
          // Extract input values from the JSON message
          std::map<std::string, std::string> inputValues;
          debug("Extracting input values from JSON message");
          if (doc.containsKey("inputValues")) {
            debug("JSON contains inputValues key");
            if (doc["inputValues"].is<JsonObject>()) {
              debug("inputValues is a JSON object");
              JsonObject inputObj = doc["inputValues"];
              debug(("inputValues object has " + std::to_string(inputObj.size()) + " entries").c_str());
              for (JsonPair kv : inputObj) {
                // Safely handle the value, even if it's null or empty
                if (kv.value().isNull()) {
                  // Handle null value - store as empty string
                  inputValues[kv.key().c_str()] = "";
                  debug(("Input value: " + std::string(kv.key().c_str()) + " = (null)").c_str());
                } else {
                  // Use String for Arduino compatibility, which handles empty strings properly
                  String arduinoValue = kv.value().as<String>();
                  std::string stdValue = arduinoValue.c_str(); // Convert to std::string
                  inputValues[kv.key().c_str()] = stdValue;
                  debug(("Input value: " + std::string(kv.key().c_str()) + " = " + stdValue).c_str());
                }
              }
            } else {
              debug("inputValues is NOT a JSON object");
            }
          } else {
            debug("JSON does NOT contain inputValues key");
            // Dump the JSON message for debugging
            std::string jsonDump;
            serializeJson(doc, jsonDump);
            debug(("JSON message: " + jsonDump).c_str());
          }
          
          // Check if this is a popup-related process type
          bool isPopupHandled = false;
          if (strlen(popupId) > 0) {
            // This might be a popup-related process type, check if the popupId matches any of our registered popups
            for (const auto& menu : menus) {
              for (const auto& item : menu.items) {
                // Check if this item's callback is a popup callback
                std::string itemPopupId = std::string("popup_") + menu.name + "_" + item.name;
                // Replace spaces with underscores
                for (size_t i = 0; i < itemPopupId.length(); i++) {
                  if (itemPopupId[i] == ' ') {
                    itemPopupId[i] = '_';
                  }
                }
                
                if (itemPopupId == popupId) {
                  // This is the menu item that created the popup
                  // Call its popup callback if it exists
                  if (item.popupCallback) {
                    debug(("Calling popup callback for: " + itemPopupId).c_str());
                    item.popupCallback(inputValues);
                    isPopupHandled = true;
                  } else {
                    debug(("No popup callback found for: " + itemPopupId).c_str());
                  }
                  break;
                }
              }
              if (isPopupHandled) break;
            }
          }
          
          // If this is a popup-related process type and we handled it, mark the event as handled
          if (isPopupHandled) {
            eventHandled = true;
          } else {
            // This is not a popup-related process type or we couldn't handle it
            // Pass it to the local event handler
            debug("Process type not handled by SPAmanager, passing to local event handler");
            if (localEventsCallback) {
              localEventsCallback(num, type, payload, length);
              eventHandled = true;
            } else {
              debug("No local event handler registered");
            }
          }
        }
        else if (doc["type"] == "custom") 
        {
          // Handle custom message type
          debug("WebSocket: custom message received");
          
          // Check if we have a local events callback registered
          if (localEventsCallback) {
              debug(("Forwarding custom message to local event handler (WStype [" + std::to_string(WStype_TEXT) + "])").c_str());
              // Forward the event to the local event handler
              localEventsCallback(num, WStype_TEXT, payload, length);
              //-no-eventHandled = true;
          } else {
              debug("No local event handler registered for custom message");
          }
        }
        else 
        {
          // Unknown message type, pass it to the local event handler
          debug(("Unknown message type: " + std::string(doc["type"] | "unknown")).c_str());
          if (localEventsCallback) 
          {
            localEventsCallback(num, type, payload, length);
            eventHandled = true;
          }
        }
    }
    
    // If no event was handled and we have a local events callback, call it
    //if (!eventHandled && localEventsCallback) 
    if (localEventsCallback) 
    {
      localEventsCallback(num, type, payload, length);
    }
} // handleWebSocketEvent()

void SPAmanager::broadcastState() 
{
    // Create a JSON document without the page content
    const size_t capacity = JSON_ARRAY_SIZE(5) +
                           JSON_ARRAY_SIZE(10) +
                           10 * JSON_OBJECT_SIZE(3) +
                           JSON_OBJECT_SIZE(10) +
                           256; // Reduced size since we're not including page content
                           
    DynamicJsonDocument doc(capacity);
    if (doc.capacity() == 0) {
        debug("Failed to allocate JSON buffer for broadcast state");
        return;
    }
    
    // Set metadata but not the content
    doc["pageName"] = activePage ? activePage->name : "";
    doc["isVisible"] = activePage ? true : false;
    doc["hasContent"] = activePage && activePage->isFileStorage;
    
    // Other state information
    doc["message"] = currentMessage;
    doc["isError"] = isError;
    doc["messageDuration"] = messageEndTime > 0 ? (messageEndTime - millis()) : 0;
    doc["isPopup"] = isPopup;
    doc["showCloseButton"] = showCloseButton;
    
    // Menu information
    JsonArray menuArray = doc.createNestedArray("menus");
    for (const auto& menu : menus) {
        if (strcmp(menu.pageName, activePage ? activePage->name : "") == 0) {
            JsonObject menuObj = menuArray.createNestedObject();
            menuObj["name"] = menu.name;
            JsonArray itemArray = menuObj.createNestedArray("items");
            
            for (const auto& item : menu.items) {
                JsonObject itemObj = itemArray.createNestedObject();
                itemObj["name"] = item.name;
                if (item.hasUrl()) {
                    itemObj["url"] = item.url;
                }
                itemObj["disabled"] = item.disabled;
            }
        }
    }
    
    std::string output;
    serializeJson(doc, output);
    
    if (!output.empty()) {
        ws.broadcastTXT(output.c_str(), output.length());
        
        // If there's an active page with content, stream it separately
        if (activePage && activePage->isFileStorage) {
            streamPageContent(*activePage);
        } else if (activePage) {
            // Send error page if there's no file storage
            const size_t errorCapacity = JSON_OBJECT_SIZE(3) + strlen(DEFAULT_ERROR_PAGE) + 50;
            DynamicJsonDocument errorDoc(errorCapacity);
            errorDoc["type"] = "pageContent";
            errorDoc["content"] = DEFAULT_ERROR_PAGE;
            
            std::string errorOutput;
            serializeJson(errorDoc, errorOutput);
            
            if (!errorOutput.empty()) {
                ws.broadcastTXT(errorOutput.c_str(), errorOutput.length());
            }
        }
    } else {
        debug("Failed to serialize JSON for broadcast state");
    }
} // broadcastState()


// method to stream page content in chunks
void SPAmanager::streamPageContent(const Page& page) 
{
    // Remove leading slash for LittleFS
    std::string filePath = page.filePath;
    if (!filePath.empty() && filePath[0] != '/') {
        filePath = "/" + filePath;
    }
    
    debug(("streamPageContent(): Streaming page content from file: " + filePath).c_str());
    File pageFile = LittleFS.open(filePath.c_str(), "r");
    if (!pageFile) {
        error(("Failed to open page file: " + filePath).c_str());
        
        // Send error page
        const size_t errorCapacity = JSON_OBJECT_SIZE(3) + strlen(DEFAULT_ERROR_PAGE) + 50;
        DynamicJsonDocument errorDoc(errorCapacity);
        errorDoc["type"] = "pageContent";
        errorDoc["content"] = DEFAULT_ERROR_PAGE;
        
        std::string errorOutput;
        serializeJson(errorDoc, errorOutput);
        
        if (!errorOutput.empty()) {
            ws.broadcastTXT(errorOutput.c_str(), errorOutput.length());
        }
        return;
    }
    
    // Stream content in chunks
    const size_t chunkSize = 1024;
    char buffer[chunkSize];
    int chunkIndex = 0;
    int totalChunks = (pageFile.size() + chunkSize - 1) / chunkSize; // Ceiling division
    
    while (pageFile.available()) {
        size_t bytesRead = pageFile.readBytes(buffer, chunkSize - 1);
        buffer[bytesRead] = '\0';
        
        // Create a JSON document for this chunk
        const size_t chunkCapacity = JSON_OBJECT_SIZE(5) + bytesRead + 50;
        DynamicJsonDocument chunkDoc(chunkCapacity);
        chunkDoc["type"] = "pageChunk";
        chunkDoc["content"] = buffer;
        chunkDoc["chunkIndex"] = chunkIndex;
        chunkDoc["totalChunks"] = totalChunks;
        chunkDoc["final"] = !pageFile.available();
        
        std::string chunkOutput;
        serializeJson(chunkDoc, chunkOutput);
        
        if (!chunkOutput.empty()) {
            ws.broadcastTXT(chunkOutput.c_str(), chunkOutput.length());
        }
        
        chunkIndex++;
        yield(); // Allow other tasks to run
    }
    
    pageFile.close();
} // streamPageContent()


// ensure the pages directory exists
bool SPAmanager::ensurePageDirectory() 
{
    std::string pagesDir = PAGES_DIRECTORY;
    if (pagesDir[0] != '/') {
        pagesDir = "/"+pagesDir; // Add leading slash for LittleFS
    }
    // Remove trailing slash if present (but not if it's just "/")
    if (pagesDir.length() > 1 && pagesDir.back() == '/') {
        pagesDir.pop_back();
    }
    if (!LittleFS.exists(pagesDir.c_str())) {
        debug(("ensurePageDirectory(): Creating [" + pagesDir + "] directory").c_str());
        if (!LittleFS.mkdir(pagesDir.c_str())) {
            error(("ensurePageDirectory(): Failed to create [" + pagesDir + "] directory").c_str());
            return false;
        }
    }
    return true;

} //  ensurePageDirectory()


// method to get page content from file
std::string SPAmanager::getPageContent(const Page& page) 
{
    if (!page.isFileStorage) {
        // For backward compatibility or error pages
        return "";
    }
    
    // Remove leading slash for LittleFS
    std::string filePath = page.filePath;
    if (!filePath.empty() && filePath[0] != '/') {
        filePath = "/"+filePath;
    }
    
    debug(("Reading page content from file: " + filePath).c_str());
    
    // Check if file exists
    if (!LittleFS.exists(filePath.c_str())) {
        error(("Page file does not exist: " + filePath).c_str());
        return DEFAULT_ERROR_PAGE;
    }
    
    File pageFile = LittleFS.open(filePath.c_str(), "r");
    if (!pageFile) {
        error(("Failed to open page file: " + filePath).c_str());
        return DEFAULT_ERROR_PAGE;
    }
    
    // Check file size
    size_t fileSize = pageFile.size();
    if (fileSize > MAX_CONTENT_LEN) {
        error(("Page file too large: " + filePath + " (" + std::to_string(fileSize) + " bytes)").c_str());
        pageFile.close();
        return DEFAULT_ERROR_PAGE;
    }
    
    std::string content;
    content.reserve(fileSize); // Pre-allocate memory
    
    while (pageFile.available()) {
        // Read in chunks to avoid memory issues
        char buffer[256];
        size_t bytesRead = pageFile.readBytes(buffer, sizeof(buffer) - 1);
        buffer[bytesRead] = '\0';
        content += buffer;
    }
    
    pageFile.close();
    return content;

} // getPageContent()


// write page content to file
bool SPAmanager::writePageToFile(const char* pageName, const char* html) 
{
    if (!ensurePageDirectory()) {
        error("Failed to create pages directory");
        return false;
    }
    
    std::string filePath = PAGES_DIRECTORY;
    filePath += pageName;
    filePath += ".html";
        
    debug(("writePageToFile(): Writing page content to file: " + filePath).c_str());
    
    // Try to open the file
    File pageFile = LittleFS.open(filePath.c_str(), "w");
    if (!pageFile) {
        error(("Failed to open page file for writing: " + filePath).c_str());
        
        // Try to ensure the directory exists again
        if (ensurePageDirectory()) 
        {
            // Try opening the file again
            pageFile = LittleFS.open(filePath.c_str(), "w");
            if (!pageFile) {
                error(("Still failed to open page file for writing: " + filePath).c_str());
                return false;
            }
        } else {
            return false;
        }
    }
    
    // Write in chunks to avoid memory issues
    const char* ptr = html;
    const size_t chunkSize = 256;
    bool writeSuccess = true;
    
    while (*ptr && writeSuccess) {
        size_t bytesToWrite = 0;
        while (bytesToWrite < chunkSize && ptr[bytesToWrite]) {
            bytesToWrite++;
        }
        
        if (bytesToWrite > 0) {
            size_t bytesWritten = pageFile.write((uint8_t*)ptr, bytesToWrite);
            if (bytesWritten != bytesToWrite) {
                error("Failed to write complete chunk to file");
                writeSuccess = false;
            }
            ptr += bytesToWrite;
        }
    }
    
    pageFile.close();
    
    // Verify the file was written correctly
    if (writeSuccess) {
        File verifyFile = LittleFS.open(filePath.c_str(), "r");
        if (verifyFile) {
            debug(("writePageToFile(): Successfully verified file: " + filePath).c_str());
            verifyFile.close();
        } else {
            error(("Failed to verify file: " + filePath).c_str());
            writeSuccess = false;
        }
    }
    
    return writeSuccess;

} //  writePageToFile()


void SPAmanager::addPage(const char* pageName, const char* html) {
    debug(("addPage() called with pageName: " + std::string(pageName)).c_str());
    
    // Store first page name if this is the first page
    if (pages.empty()) {
        firstPageName = pageName;
    }
    
    // Check if page already exists
    auto it = std::find_if(pages.begin(), pages.end(),
        [pageName](const Page& page) { return strcmp(page.name, pageName) == 0; });
    
    if (it != pages.end()) {
        // Page exists, update content
        if (writePageToFile(pageName, html)) {
            // Update file path if needed
            std::string filePath = PAGES_DIRECTORY;
            filePath += pageName;
            filePath += ".html";
            
            // Ensure the path starts with a slash
            if (filePath[0] != '/') {
                filePath = "/" + filePath;
            }
            
            it->setFilePath(filePath.c_str());
            updateClients();
        } else {
            error(("Failed to update page file for: " + std::string(pageName)).c_str());
        }
    } else {
        // New page
        if (writePageToFile(pageName, html)) {
            Page page;
            page.setName(pageName);
            
            std::string filePath = PAGES_DIRECTORY;
            filePath += pageName;
            filePath += ".html";
            
            // Ensure the path starts with a slash
            if (filePath[0] != '/') {
                filePath = "/" + filePath;
            }
            
            page.setFilePath(filePath.c_str());
            
            page.isVisible = false;
            pages.push_back(page);
            
            if (!activePage) {
                activePage = &pages.back();
                pages.back().isVisible = true;
                setHeaderTitle(pages.back().title);
                updateClients();
            }
        } else {
            error(("Failed to create page file for: " + std::string(pageName)).c_str());
            // Add error page instead
            Page page;
            page.setName(pageName);
            page.isFileStorage = false;
            page.isVisible = false;
            pages.push_back(page);
            
            if (!activePage) {
                activePage = &pages.back();
                pages.back().isVisible = true;
                setHeaderTitle(pages.back().title);
                updateClients();
            }
        }
    }
} // addPage()



void SPAmanager::setPageTitle(const char* pageName, const char* title)
{
  debug(("setPageTitle() called with pageName: " + std::string(pageName) + ", title: " + std::string(title)).c_str());
  for (auto& page : pages) 
  {
    if (strcmp(page.name, pageName) == 0) 
    {
      page.setTitle(title);
      if (activePage && strcmp(activePage->name, pageName) == 0) 
      {
        setHeaderTitle(title);
      }
      break;
    }
  }
} // setPageTitle()


template <typename T>
void SPAmanager::setPlaceholder(const char* pageName, const char* placeholder, T value) {
    debug((std::string("setPlaceholder() called with pageName: ") + pageName + ", placeholder: " + placeholder).c_str());
    
    for (auto& page : pages) {
        if (strcmp(page.name, pageName) == 0) {
            if (!page.isFileStorage) {
                debug("Page is not using file storage, skipping placeholder update");
                return;
            }
            
            // Read the current content from file
            std::string content = getPageContent(page);
            if (content.empty() || content == DEFAULT_ERROR_PAGE) {
                debug("Failed to read page content or using error page");
                return;
            }
            
            std::string idStr1 = std::string("id='") + placeholder + "'";
            std::string idStr2 = std::string("id=\"") + placeholder + "\"";
            size_t pos = content.find(idStr1);
            if (pos == std::string::npos) {
                pos = content.find(idStr2);
            }
            
            if (pos != std::string::npos) {
                // Check if it's an input field
                size_t inputStart = content.rfind("<input", pos);
                if (inputStart != std::string::npos && inputStart < pos) {
                    // Find value attribute or closing bracket
                    size_t valueStart1 = content.find("value='", inputStart);
                    size_t valueStart2 = content.find("value=\"", inputStart);
                    size_t closingBracket = content.find(">", inputStart);
                    
                    if (valueStart1 != std::string::npos && valueStart1 < closingBracket) {
                        // Value attribute exists with single quotes
                        size_t valueEnd = content.find("'", valueStart1 + 7);
                        if (valueEnd != std::string::npos) {
                            content.replace(valueStart1 + 7, valueEnd - (valueStart1 + 7), std::to_string(value));
                        }
                    } else if (valueStart2 != std::string::npos && valueStart2 < closingBracket) {
                        // Value attribute exists with double quotes
                        size_t valueEnd = content.find("\"", valueStart2 + 7);
                        if (valueEnd != std::string::npos) {
                            content.replace(valueStart2 + 7, valueEnd - (valueStart2 + 7), std::to_string(value));
                        }
                    } else {
                        // No value attribute, insert before closing bracket
                        content.insert(closingBracket, " value=\"" + std::to_string(value) + "\"");
                    }
                } else {
                    // For non-input elements
                    size_t start = content.find('>', pos) + 1;
                    size_t end = content.find('<', start);
                    if (start != std::string::npos && end != std::string::npos) {
                        content.replace(start, end - start, std::to_string(value));
                    }
                }
                
                // Write the updated content back to the file
                File pageFile = LittleFS.open(page.filePath, "w");
                if (pageFile) {
                    pageFile.print(content.c_str());
                    pageFile.close();
                    
                    // If this is the active page, update clients
                    if (activePage && strcmp(activePage->name, pageName) == 0) {
                        const size_t capacity = JSON_OBJECT_SIZE(3) + 256;
                        DynamicJsonDocument doc(capacity);
                        
                        doc["type"] = "update";
                        doc["target"] = placeholder;
                        doc["content"] = std::to_string(value);
                        
                        std::string output;
                        serializeJson(doc, output);
                        
                        if (!output.empty()) {
                            ws.broadcastTXT(output.c_str(), output.length());
                        }
                    }
                } else {
                    error(("Failed to open page file for writing: " + std::string(page.filePath)).c_str());
                }
            }
            break;
        }
    }
} // setPlaceholder()

// Explicit specialization for char* to handle string literals
template <>
void SPAmanager::setPlaceholder<const char*>(const char* pageName, const char* placeholder, const char* value) {
    debug((std::string("setPlaceholder() called with pageName: ") + pageName + ", placeholder: " + placeholder + ", value: " + value).c_str());
    
    for (auto& page : pages) {
        if (strcmp(page.name, pageName) == 0) {
            if (!page.isFileStorage) {
                debug("Page is not using file storage, skipping placeholder update");
                return;
            }
            
            // Read the current content from file
            std::string content = getPageContent(page);
            if (content.empty() || content == DEFAULT_ERROR_PAGE) {
                debug("Failed to read page content or using error page");
                return;
            }
            
            std::string idStr1 = std::string("id='") + placeholder + "'";
            std::string idStr2 = std::string("id=\"") + placeholder + "\"";
            size_t pos = content.find(idStr1);
            if (pos == std::string::npos) {
                pos = content.find(idStr2);
            }
            
            if (pos != std::string::npos) {
                // Check if it's an input field
                size_t inputStart = content.rfind("<input", pos);
                if (inputStart != std::string::npos && inputStart < pos) {
                    // Find value attribute or closing bracket
                    size_t valueStart1 = content.find("value='", inputStart);
                    size_t valueStart2 = content.find("value=\"", inputStart);
                    size_t closingBracket = content.find(">", inputStart);
                    
                    if (valueStart1 != std::string::npos && valueStart1 < closingBracket) {
                        // Value attribute exists with single quotes
                        size_t valueEnd = content.find("'", valueStart1 + 7);
                        if (valueEnd != std::string::npos) {
                            content.replace(valueStart1 + 7, valueEnd - (valueStart1 + 7), value);
                        }
                    } else if (valueStart2 != std::string::npos && valueStart2 < closingBracket) {
                        // Value attribute exists with double quotes
                        size_t valueEnd = content.find("\"", valueStart2 + 7);
                        if (valueEnd != std::string::npos) {
                            content.replace(valueStart2 + 7, valueEnd - (valueStart2 + 7), value);
                        }
                    } else {
                        // No value attribute, insert before closing bracket
                        content.insert(closingBracket, " value=\"" + std::string(value) + "\"");
                    }
                } else {
                    // For non-input elements
                    size_t start = content.find('>', pos) + 1;
                    size_t end = content.find('<', start);
                    if (start != std::string::npos && end != std::string::npos) {
                        content.replace(start, end - start, value);
                    }
                }
                
                // Write the updated content back to the file
                File pageFile = LittleFS.open(page.filePath, "w");
                if (pageFile) {
                    pageFile.print(content.c_str());
                    pageFile.close();
                    
                    // If this is the active page, update clients
                    if (activePage && strcmp(activePage->name, pageName) == 0) {
                        const size_t capacity = JSON_OBJECT_SIZE(3) + 256;
                        DynamicJsonDocument doc(capacity);
                        
                        doc["type"] = "update";
                        doc["target"] = placeholder;
                        doc["content"] = value;
                        
                        std::string output;
                        serializeJson(doc, output);
                        
                        if (!output.empty()) {
                            ws.broadcastTXT(output.c_str(), output.length());
                        }
                    }
                } else {
                    error(("Failed to open page file for writing: " + std::string(page.filePath)).c_str());
                }
            }
            break;
        }
    }
} // setPlaceholder()

SPAmanager::PlaceholderValue SPAmanager::getPlaceholder(const char* pageName, const char* placeholder) {
    debug((std::string("getPlaceholder() called with pageName: ") + pageName + ", placeholder: " + placeholder).c_str());
    std::string value = "";
    
    for (const auto& page : pages) {
        if (strcmp(page.name, pageName) == 0) {
            if (!page.isFileStorage) {
                debug("Page is not using file storage, returning empty placeholder");
                return PlaceholderValue("");
            }
            
            // Read the current content from file
            std::string content = getPageContent(page);
            if (content.empty() || content == DEFAULT_ERROR_PAGE) {
                debug("Failed to read page content or using error page");
                return PlaceholderValue("");
            }
            
            std::string idStr1 = std::string("id='") + placeholder + "'";
            std::string idStr2 = std::string("id=\"") + placeholder + "\"";
            size_t pos = content.find(idStr1);
            if (pos == std::string::npos) {
                pos = content.find(idStr2);
            }
            
            if (pos != std::string::npos) {
                // Check if it's an input field
                size_t inputStart = content.rfind("<input", pos);
                if (inputStart != std::string::npos && inputStart < pos) {
                    // Find value attribute with single or double quotes
                    size_t valueStart1 = content.find("value='", inputStart);
                    size_t valueStart2 = content.find("value=\"", inputStart);
                    size_t closingBracket = content.find(">", inputStart);
                    
                    if (valueStart1 != std::string::npos && valueStart1 < closingBracket) {
                        valueStart1 += 7; // Length of "value='"
                        size_t valueEnd = content.find("'", valueStart1);
                        if (valueEnd != std::string::npos) {
                            value = content.substr(valueStart1, valueEnd - valueStart1);
                        }
                    } else if (valueStart2 != std::string::npos && valueStart2 < closingBracket) {
                        valueStart2 += 7; // Length of "value=\""
                        size_t valueEnd = content.find("\"", valueStart2);
                        if (valueEnd != std::string::npos) {
                            value = content.substr(valueStart2, valueEnd - valueStart2);
                        }
                    }
                } else {
                    // For non-input elements, get content between tags
                    size_t start = content.find('>', pos) + 1;
                    size_t end = content.find('<', start);
                    
                    if (start != std::string::npos && end != std::string::npos) {
                        value = content.substr(start, end - start);
                    }
                }
                // Trim whitespace
                value.erase(0, value.find_first_not_of(" \t\n\r\f\v"));
                value.erase(value.find_last_not_of(" \t\n\r\f\v") + 1);
            }
            break;
        }
    }
    
    return PlaceholderValue(value.c_str());
} // PlaceholderValue getPlaceholder()


// Explicit template instantiations for setPlaceholder
template void SPAmanager::setPlaceholder<unsigned int>(const char*, const char*, unsigned int);
template void SPAmanager::setPlaceholder<int>(const char*, const char*, int);
template void SPAmanager::setPlaceholder<float>(const char*, const char*, float);
template void SPAmanager::setPlaceholder<double>(const char*, const char*, double);

void SPAmanager::activatePage(const char* pageName) 
{
    debug(("activatePage() called with pageName: " + std::string(pageName)).c_str());
    
    // Check if the page exists
    if (!pageExists(pageName))
    {
      error(("ERROR: Page [" + std::string(pageName) + "] does not exist").c_str());
      return;
    }
    
    for (auto& page : pages) 
    {
        bool shouldBeVisible = (strcmp(page.name, pageName) == 0);
        if (shouldBeVisible) 
        {
            activePage = &page;
            activePageName = pageName; //store activePageName
            setHeaderTitle(page.title);
            Serial.printf("Activating page: %s with [%s]\n", page.name, page.title);
        }
        page.isVisible = shouldBeVisible;
    }
    updateClients();
}

std::string SPAmanager::getActivePageName() const
{
  return activePage ? activePage->name : "";

}

void SPAmanager::addMenu(const char* pageName, const char* menuName) 
{
    debug(("addMenu() called with pageName: " + std::string(pageName) + ", menuName: " + std::string(menuName)).c_str());
    
    // Check if the page exists
    if (!pageExists(pageName))
    {
      error(("addMenu(): ERROR: Page [" + std::string(pageName) + "] does not exist").c_str());
      return;
    }
    
    Menu menu;
    menu.setName(menuName);
    menu.setPageName(pageName);
    menus.push_back(menu);
    if (activePage && strcmp(activePage->name, pageName) == 0) {
        updateClients();
    }
}

void SPAmanager::addMenuItem(const char* pageName, const char* menuName, const char* itemName, std::function<void()> callback) 
{
    debug(("addMenuItem() called with pageName: " + std::string(pageName) + ", menuName: " + std::string(menuName) + ", itemName: " + std::string(itemName) + " (callback)").c_str());
    
    // Check if the page exists
    if (!pageExists(pageName))
    {
      error(("addMenuItem(): ERROR: Page [" + std::string(pageName) + "] does not exist").c_str());
      return;
    }
    
    // Check if the menu exists
    if (!menuExists(pageName, menuName))
    {
      error(("addMenuItem(): ERROR: Menu [" + std::string(menuName) + "] does not exist on page [" + std::string(pageName) + "]").c_str());
      return;
    }
    
    for (auto& menu : menus) 
    {
        if (strcmp(menu.name, menuName) == 0 && strcmp(menu.pageName, pageName) == 0) 
        {
            MenuItem item;
            item.setName(itemName);
            item.setUrl(nullptr);
            item.callback = callback;
            menu.items.push_back(item);
            if (activePage && strcmp(activePage->name, pageName) == 0) {
                updateClients();
            }
            break;
        }
    }
}

void SPAmanager::addMenuItem(const char* pageName, const char* menuName, const char* itemName, const char* url) 
{
    debug(("addMenuItem() called with pageName: " + std::string(pageName) + ", menuName: " + std::string(menuName) + ", itemName: " + std::string(itemName) + ", url: " + std::string(url)).c_str());
    
    // Check if the page exists
    if (!pageExists(pageName))
    {
      error(("addMenuItem(): ERROR: Page [" + std::string(pageName) + "] does not exist").c_str());
      return;
    }
    
    // Check if the menu exists
    if (!menuExists(pageName, menuName))
    {
      error(("addMenuItem(): ERROR: Menu [" + std::string(menuName) + "] does not exist on page [" + std::string(pageName) + "]").c_str());
      return;
    }
    
    for (auto& menu : menus) 
    {
        if (strcmp(menu.name, menuName) == 0 && strcmp(menu.pageName, pageName) == 0) 
        {
            MenuItem item;
            item.setName(itemName);
            item.setUrl(url);
            menu.items.push_back(item);
            if (activePage && strcmp(activePage->name, pageName) == 0) {
                updateClients();
            }
            break;
        }
    }
}

void SPAmanager::addMenuItem(const char* pageName, const char* menuName, const char* itemName, std::function<void(const char*)> callback, const char* param) 
{
  debug(("addMenuItem() called with pageName: " + std::string(pageName) + ", menuName: " + std::string(menuName) + ", itemName: " + std::string(itemName) + " (callback with param)").c_str());
  
  // Check if the page exists
  if (!pageExists(pageName))
  {
    error(("addMenuItem(): ERROR: Page [" + std::string(pageName) + "] does not exist").c_str());
    return;
  }
  
  // Check if the menu exists
  if (!menuExists(pageName, menuName))
  {
    error(("addMenuItem(): ERROR: Menu [" + std::string(menuName) + "] does not exist on page [" + std::string(pageName) + "]").c_str());
    return;
  }
  
  for (auto& menu : menus) 
  {
    if (strcmp(menu.name, menuName) == 0 && strcmp(menu.pageName, pageName) == 0) 
    {
      MenuItem item;
      item.setName(itemName);
      item.setUrl(nullptr);
      item.callback = [callback, param]() { callback(param); };
      menu.items.push_back(item);
      if (activePage && strcmp(activePage->name, pageName) == 0) {
        updateClients();
      }
      break;
    }
  }
}

void SPAmanager::addMenuItemPopup(const char* pageName, const char* menuName, const char* menuItem, const char* popupMenu, std::function<void(const std::map<std::string, std::string>&)> callback)
{
  debug(("addMenuItemPopup() called with pageName: " + std::string(pageName) + ", menuName: " + std::string(menuName) + ", menuItem: " + std::string(menuItem)).c_str());
  
  // Check if the page exists
  if (!pageExists(pageName))
  {
    error(("addMenuItemPopup(): ERROR: Page [" + std::string(pageName) + "] does not exist").c_str());
    return;
  }
  
  // Check if the menu exists
  if (!menuExists(pageName, menuName))
  {
    error(("addMenuItemPopup(): ERROR: Menu [" + std::string(menuName) + "] does not exist on page [" + std::string(pageName) + "]").c_str());
    return;
  }
  
  for (auto& menu : menus)
  {
    if (strcmp(menu.name, menuName) == 0 && strcmp(menu.pageName, pageName) == 0)
    {
      // Create a unique ID for this popup
      std::string popupId = std::string("popup_") + menuName + "_" + menuItem;
      
      // Replace any spaces with underscores in the ID
      for (size_t i = 0; i < popupId.length(); i++)
      {
        if (popupId[i] == ' ')
        {
          popupId[i] = '_';
        }
      }
      
      // Create a menu item with a callback that calls the show popup function
      MenuItem item;
      item.setName(menuItem);
      item.setUrl(nullptr);
      item.popupCallback = callback;
      
      // Create a lambda that will call our JavaScript function with the popup content
      item.callback = [this, popupId, popupMenu]() {
        // Create a JSON object with the popup content and configuration
        const size_t capacity = JSON_OBJECT_SIZE(3) + strlen(popupMenu) + 256;
        DynamicJsonDocument doc(capacity);
        
        doc["event"] = "showPopup";
        doc["id"] = popupId;
        doc["content"] = popupMenu;
        
        std::string output;
        serializeJson(doc, output);
        
        if (!output.empty() && hasConnectedClient)
        {
          ws.broadcastTXT(output.c_str(), output.length());
        }
      };
      
      menu.items.push_back(item);
      break;
    }
  }
}


void SPAmanager::enableMenuItem(const char* pageName, const char* menuName, const char* itemName)
{
    debug(("enableMenuItem() called with pageName: " + std::string(pageName) + ", menuName: " + std::string(menuName) + ", itemName: " + std::string(itemName)).c_str());
    
    // Check if the page exists
    if (!pageExists(pageName))
    {
      error(("enableMenuItem(): ERROR: Page [" + std::string(pageName) + "] does not exist").c_str());
      return;
    }
    
    // Check if the menu exists
    if (!menuExists(pageName, menuName))
    {
      error(("enableMenuItem(): ERROR: Menu [" + std::string(menuName) + "] does not exist on page [" + std::string(pageName) + "]").c_str());
      return;
    }
    
    for (auto& menu : menus) 
    {
        if (strcmp(menu.name, menuName) == 0 && strcmp(menu.pageName, pageName) == 0) 
        {
            for (auto& item : menu.items) 
            {
                if (strcmp(item.name, itemName) == 0) 
                {
                    item.disabled = false;
                    if (activePage && strcmp(activePage->name, pageName) == 0) 
                    {
                        updateClients();
                    }
                    break;
                }
            }
            break;
        }
    }
}

void SPAmanager::disableMenuItem(const char* pageName, const char* menuName, const char* itemName)
{
    debug(("disableMenuItem() called with pageName: " + std::string(pageName) + ", menuName: " + std::string(menuName) + ", itemName: " + std::string(itemName)).c_str());
    
    // Check if the page exists
    if (!pageExists(pageName))
    {
      error(("disableMenuItem(): ERROR: Page [" + std::string(pageName) + "] does not exist").c_str());
      return;
    }
    
    // Check if the menu exists
    if (!menuExists(pageName, menuName))
    {
      error(("disableMenuItem(): ERROR: Menu [" + std::string(menuName) + "] does not exist on page [" + std::string(pageName) + "]").c_str());
      return;
    }
    
    for (auto& menu : menus) 
    {
        if (strcmp(menu.name, menuName) == 0 && strcmp(menu.pageName, pageName) == 0) 
        {
            for (auto& item : menu.items) 
            {
                if (strcmp(item.name, itemName) == 0) 
                {
                    item.disabled = true;
                    if (activePage && strcmp(activePage->name, pageName) == 0) 
                    {
                        updateClients();
                    }
                    break;
                }
            }
            break;
        }
    }
}

void SPAmanager::enableID(const char* pageName, const char* id)
{
  debug(("enableID() called with pageName: " + std::string(pageName) + ", id: " + std::string(id)).c_str());
  for (auto& page : pages)
  {
    if (strcmp(page.name, pageName) == 0)
    {
      if (!page.isFileStorage) {
        debug("Page is not using file storage, skipping ID update");
        return;
      }
      
      // Read the current content from file
      std::string content = getPageContent(page);
      if (content.empty() || content == DEFAULT_ERROR_PAGE) {
        debug("Failed to read page content or using error page");
        return;
      }
      
      std::string idStr1 = std::string("id='") + id + "'";
      std::string idStr2 = std::string("id=\"") + id + "\"";
      size_t pos = content.find(idStr1);
      if (pos == std::string::npos)
      {
        pos = content.find(idStr2);
      }
      
      if (pos != std::string::npos)
      {
        // Find the style attribute or the end of the tag
        size_t stylePos = content.find("style", pos);
        size_t tagEnd = content.find(">", pos);
        
        if (stylePos != std::string::npos && stylePos < tagEnd)
        {
          // Style attribute exists, update display property
          size_t displayPos = content.find("display:", stylePos);
          if (displayPos != std::string::npos && displayPos < tagEnd)
          {
            // Replace existing display value
            size_t valueStart = displayPos + 8;
            size_t valueEnd = content.find(";", valueStart);
            if (valueEnd == std::string::npos || valueEnd > tagEnd)
            {
              valueEnd = content.find("\"", valueStart);
            }
            if (valueEnd != std::string::npos && valueEnd > valueStart)
            {
              content.replace(valueStart, valueEnd - valueStart, "block");
            }
          }
          else
          {
            // Add display property to existing style
            size_t quotePos = content.find("\"", stylePos);
            if (quotePos != std::string::npos && quotePos < tagEnd)
            {
              content.insert(quotePos, ";display:block");
            }
          }
        }
        else
        {
          // No style attribute, add it before the end of the tag
          content.insert(tagEnd, " style=\"display:block\"");
        }
        
        // Write the updated content back to the file
        File pageFile = LittleFS.open(page.filePath, "w");
        if (pageFile) {
          pageFile.print(content.c_str());
          pageFile.close();
          
          if (activePage && strcmp(activePage->name, pageName) == 0)
          {
            updateClients();
          }
        } else {
          error(("Failed to open page file for writing: " + std::string(page.filePath)).c_str());
        }
      }
      break;
    }
  }
} // enableID()

void SPAmanager::disableID(const char* pageName, const char* id)
{
  debug(("disableID() called with pageName: " + std::string(pageName) + ", id: " + std::string(id)).c_str());
  for (auto& page : pages)
  {
    if (strcmp(page.name, pageName) == 0)
    {
      if (!page.isFileStorage) {
        debug("Page is not using file storage, skipping ID update");
        return;
      }
      
      // Read the current content from file
      std::string content = getPageContent(page);
      if (content.empty() || content == DEFAULT_ERROR_PAGE) {
        debug("Failed to read page content or using error page");
        return;
      }
      
      std::string idStr1 = std::string("id='") + id + "'";
      std::string idStr2 = std::string("id=\"") + id + "\"";
      size_t pos = content.find(idStr1);
      if (pos == std::string::npos)
      {
        pos = content.find(idStr2);
      }
      
      if (pos != std::string::npos)
      {
        // Find the style attribute or the end of the tag
        size_t stylePos = content.find("style", pos);
        size_t tagEnd = content.find(">", pos);
        
        if (stylePos != std::string::npos && stylePos < tagEnd)
        {
          // Style attribute exists, update display property
          size_t displayPos = content.find("display:", stylePos);
          if (displayPos != std::string::npos && displayPos < tagEnd)
          {
            // Replace existing display value
            size_t valueStart = displayPos + 8;
            size_t valueEnd = content.find(";", valueStart);
            if (valueEnd == std::string::npos || valueEnd > tagEnd)
            {
              valueEnd = content.find("\"", valueStart);
            }
            if (valueEnd != std::string::npos && valueEnd > valueStart)
            {
              content.replace(valueStart, valueEnd - valueStart, "none");
            }
          }
          else
          {
            // Add display property to existing style
            size_t quotePos = content.find("\"", stylePos);
            if (quotePos != std::string::npos && quotePos < tagEnd)
            {
              content.insert(quotePos, ";display:none");
            }
          }
        }
        else
        {
          // No style attribute, add it before the end of the tag
          content.insert(tagEnd, " style=\"display:none\"");
        }
        
        // Write the updated content back to the file
        File pageFile = LittleFS.open(page.filePath, "w");
        if (pageFile) {
          pageFile.print(content.c_str());
          pageFile.close();
          
          if (activePage && strcmp(activePage->name, pageName) == 0)
          {
            updateClients();
          }
        } else {
          error(("Failed to open page file for writing: " + std::string(page.filePath)).c_str());
        }
      }
      break;
    }
  }
} // disableID()



void SPAmanager::includeJsFile(const std::string &path2JsFile)
{
  std::string sanitizedJsPath = path2JsFile;
  
  debug(("SPAmanager::includeJsFile() called with path2JsFile: [" + sanitizedJsPath + "]").c_str());

  //-- Reject a single slash as an invalid path
  if (sanitizedJsPath == "/") 
  {
    error("ERROR: path2JsFile cannot be '/'");
    return;
  }

  //-- Ensure path2JsFile starts with '/'
  if (!sanitizedJsPath.empty() && sanitizedJsPath[0] != '/')
  {
    sanitizedJsPath.insert(0, 1, '/');
  }

  //-- Replace double slashes '//' with single '/'
  for (size_t pos = 0; (pos = sanitizedJsPath.find("//", pos)) != std::string::npos; )
  {
    sanitizedJsPath.erase(pos, 1);
  }

  //-- Remove trailing slash (unless it's just "/")
  if (sanitizedJsPath.length() > 1 && sanitizedJsPath.back() == '/')
  {
    sanitizedJsPath.pop_back();
  }

  //-- Check if file has already been served
  if (servedFiles.find(sanitizedJsPath) != servedFiles.end()) 
  {
    debug(("includeJsFile(): Script [" + sanitizedJsPath + "] already served, skipping").c_str());
    return;
  }

  debug(("includeJsFile(): Adding script to servedFiles: [" + sanitizedJsPath + "]").c_str());

  //-- Extract `fName` (last part of the path) for `serveStatic()`
  std::string fName = sanitizedJsPath.substr(sanitizedJsPath.find_last_of('/') + 1);
  if (!fName.empty() && fName[0] != '/')
  {
    fName.insert(0, 1, '/');
  }

  //-- Prepare the filesystem path (remove leading slash)
  //std::string fsPath = sanitizedJsPath;
  //if (!fsPath.empty() && fsPath[0] == '/') {
  //  fsPath = fsPath.substr(1);
  //}

  //-- Check if the file exists
  if (!LittleFS.exists(sanitizedJsPath.c_str())) {
    error(("File does not exist: " + sanitizedJsPath).c_str());
    return;
  }

  //-- Debugging `serveStatic()` call
  debug(("includeJsFile(): server.serveStatic(" + fName + ", LittleFS, " + sanitizedJsPath + ")").c_str());

  //-- Serve the script file statically
  server.serveStatic(fName.c_str(), LittleFS, sanitizedJsPath.c_str());

  //-- Add to served files
  servedFiles.insert(sanitizedJsPath);

} // includeJsFile()

void SPAmanager::includeCssFile(const std::string &path2CssFile)
{
  std::string sanitizedCssPath = path2CssFile;
  
  debug(("SPAmanager::includeCssFile() called with path2CssFile: [" + sanitizedCssPath + "]").c_str());

  //-- Reject a single slash as an invalid path
  if (sanitizedCssPath == "/") 
  {
    error("ERROR: path2CssFile cannot be '/'");
    return;
  }

  //-- Ensure path2CssFile starts with '/'
  if (!sanitizedCssPath.empty() && sanitizedCssPath[0] != '/')
  {
    sanitizedCssPath.insert(0, 1, '/');
  }

  //-- Replace double slashes '//' with single '/'
  for (size_t pos = 0; (pos = sanitizedCssPath.find("//", pos)) != std::string::npos; )
  {
    sanitizedCssPath.erase(pos, 1);
  }

  //-- Remove trailing slash (unless it's just "/")
  if (sanitizedCssPath.length() > 1 && sanitizedCssPath.back() == '/')
  {
    sanitizedCssPath.pop_back();
  }

  //-- Check if CSS file has already been served
  if (servedFiles.find(sanitizedCssPath) != servedFiles.end()) 
  {
    debug(("includeCssFile(): CSS [" + sanitizedCssPath + "] already served, skipping").c_str());
    return;
  }

  debug(("includeCssFile(): Adding CSS to servedFiles: [" + sanitizedCssPath + "]").c_str());

  //-- Extract `fName` (last part of the path) for `serveStatic()`
  std::string fName = sanitizedCssPath.substr(sanitizedCssPath.find_last_of('/') + 1);
  if (!fName.empty() && fName[0] != '/')
  {
    fName.insert(0, 1, '/');
  }

  //-- Prepare the filesystem path (remove leading slash)
  //std::string fsPath = sanitizedCssPath;
  //if (!fsPath.empty() && fsPath[0] == '/') {
  //  fsPath = fsPath.substr(1);
  //}

  //-- Check if the file exists
  if (!filesystemAvailable) {
    error(("filesystem unavailable, CSS file: " + sanitizedCssPath).c_str());
    //return;
  }
  if (!LittleFS.exists(sanitizedCssPath.c_str())) {
    error(("CSS file does not exist: " + sanitizedCssPath).c_str());
    return;
  }

  //-- Debugging `serveStatic()` call
  debug(("includeCssFile(): server.serveStatic(" + fName + ", LittleFS, " + sanitizedCssPath + ")").c_str());

  //-- Serve the CSS file statically
  server.serveStatic(fName.c_str(), LittleFS, sanitizedCssPath.c_str());

  //-- Add to served files
  servedFiles.insert(sanitizedCssPath);

} // includeCssFile()


void SPAmanager::callJsFunction(const char* functionName)
{
  debug(("SPAmanager::callJsFunction() called with function: " + std::string(functionName)).c_str());
  if (hasConnectedClient)
  {
    const size_t capacity = JSON_OBJECT_SIZE(2); // Two key-value pairs
    DynamicJsonDocument doc(capacity);

    // Adjusted structure to match JavaScript expectation
    doc["event"] = "callJsFunction";
    doc["data"]  = functionName;

    std::string output;
    serializeJson(doc, output);

    if (!output.empty())
    {
      ws.broadcastTXT(output.c_str(), output.length());
    }
  }
} // callJsFunction()

void SPAmanager::callJsFunction(const char* functionName, const char* parameter)
{
  debug(("SPAmanager::callJsFunction() called with function: " + std::string(functionName) + ", parameter: " + std::string(parameter)).c_str());
  if (hasConnectedClient)
  {
    const size_t capacity = JSON_OBJECT_SIZE(3); // Three key-value pairs
    DynamicJsonDocument doc(capacity);

    // Adjusted structure to match JavaScript expectation
    doc["event"] = "callJsFunction";
    doc["data"]  = functionName;
    doc["params"] = parameter;

    std::string output;
    serializeJson(doc, output);

    if (!output.empty())
    {
      ws.broadcastTXT(output.c_str(), output.length());
    }
  }
} // callJsFunction() - parameterized version

void SPAmanager::handleJsFunctionResult(const char* functionName, bool success)
{
  // Check if functionName is null or empty
  if (!functionName || functionName[0] == '\0')
  {
    if (success)
    {
      debug("JavaScript function [unknown] executed successfully");
    }
    else
    {
      error("JavaScript function [unknown] not found or failed to execute");
    }
    return;
  }
  
  // Safe to use functionName now
  if (success)
  {
    debug(("JavaScript function [" + std::string(functionName) + "] executed successfully").c_str());
  }
  else
  {
    error(("JavaScript function [" + std::string(functionName) + "] not found or failed to execute").c_str());
  }
}


void SPAmanager::setMessage(const char* message, int duration) 
{
    debug(("setMessage() called with message: " + std::string(message) + ", duration: " + std::to_string(duration)).c_str());
    strncpy(currentMessage, message, MAX_MESSAGE_LEN-1);
    currentMessage[MAX_MESSAGE_LEN-1] = '\0';
    isError = false;
    messageEndTime = duration > 0 ? millis() + (duration * 1000) : 0;
    updateClients();
}

void SPAmanager::setErrorMessage(const char* message, int duration) 
{
    debug(("setErrorMessage() called with message: " + std::string(message) + ", duration: " + std::to_string(duration)).c_str());
    strncpy(currentMessage, message, MAX_MESSAGE_LEN-1);
    currentMessage[MAX_MESSAGE_LEN-1] = '\0';
    isError = true;
    messageEndTime = duration > 0 ? millis() + (duration * 1000) : 0;
    updateClients();
}

void SPAmanager::setPopupMessage(const char* message, uint8_t duration) 
{
  debug(("setPopupMessage() called with message: " + std::string(message) + ", duration: " + std::to_string(duration)).c_str());
  strncpy(currentMessage, message, MAX_MESSAGE_LEN-1);
  currentMessage[MAX_MESSAGE_LEN-1] = '\0';
  isError = false;
  isPopup = true;
  showCloseButton = (duration == 0);
  messageEndTime = duration > 0 ? millis() + (duration * 1000) : 0;
  updateClients();
}

void SPAmanager::updateClients() 
{
    if (messageEndTime > 0 && millis() >= messageEndTime) 
    {
        currentMessage[0] = '\0';
        messageEndTime = 0;
        isPopup = false;
        showCloseButton = false;
    }
    broadcastState();
}

void SPAmanager::debug(const char* message) 
{
    if (debugOut && doDebug) 
    {
      std::string debugMessage = "SPAmanager:: " + std::string(message);
      debugOut->println(debugMessage.c_str());
    }
}

void SPAmanager::error(const char* message) 
{
  std::string debugMessage = "SPAmanager:: " + std::string(message);
  debugOut->println(debugMessage.c_str());
}

// Helper method to check if a page exists
bool SPAmanager::pageExists(const char* pageName) const
{
  for (const auto& page : pages)
  {
    if (strcmp(page.name, pageName) == 0)
    {
      return true;
    }
  }
  return false;
}

// Helper method to check if a menu exists on a specific page
bool SPAmanager::menuExists(const char* pageName, const char* menuName) const
{
  for (const auto& menu : menus)
  {
    if (strcmp(menu.pageName, pageName) == 0 && strcmp(menu.name, menuName) == 0)
    {
      return true;
    }
  }
  return false;
}


void SPAmanager::pageIsLoaded(std::function<void()> callback)
{
  debug("pageIsLoaded() called");
  pageLoadedCallback = callback;
}

void SPAmanager::setHeaderTitle(const char* title)
{
  debug(("setHeaderTitle() called with title: " + std::string(title)).c_str());
  if (hasConnectedClient)
  {
    const size_t capacity = JSON_OBJECT_SIZE(3) + 256;
    DynamicJsonDocument doc(capacity);
    
    doc["type"] = "update";
    doc["target"] = "title";
    doc["content"] = title;
    
    std::string output;
    serializeJson(doc, output);
    
    if (!output.empty())
    {
      ws.broadcastTXT(output.c_str(), output.length());
    }
  }
}


// Add these constants to SPAmanager.cpp
const char* SPAmanager::MINIMAL_HTML = R"HTML(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>SPA Manager</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 0; padding: 0; }
        .header { background-color: #333; color: white; padding: 10px; }
        .content { padding: 20px; }
        .message { padding: 10px; margin: 10px 0; border-radius: 5px; }
        .normal-message { background-color: #d4edda; color: #155724; }
        .error-message { background-color: #f8d7da; color: #721c24; }
    </style>
</head>
<body>
    <div class="header">
        <h1 id="title">SPA Manager</h1>
        <div id="datetime"></div>
    </div>
    <div id="message" class="message"></div>
    <div id="bodyContent" class="content">Loading...</div>
    <script>
        let ws = new WebSocket('ws://' + window.location.hostname + ':81');
        ws.onopen = () => {
            ws.send(JSON.stringify({type: 'pageLoaded'}));
        };
        ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                if (data.body) {
                    document.getElementById('bodyContent').innerHTML = data.body;
                }
                if (data.message) {
                    const msg = document.getElementById('message');
                    msg.textContent = data.message;
                    msg.className = data.isError ? 'message error-message' : 'message normal-message';
                }
            } catch (e) {
                console.error('Error parsing message:', e);
            }
        };
        ws.onclose = () => setTimeout(() => location.reload(), 1000);
        
        function updateDateTime() {
            const now = new Date();
            document.getElementById('datetime').textContent = now.toLocaleString();
        }
        setInterval(updateDateTime, 1000);
        updateDateTime();
    </script>
</body>
</html>
)HTML";

// Update the generateHTML method to use the minimal HTML if needed
std::string SPAmanager::generateHTML()
{
    debug(("generateHTML() called (systemFiles are in [" + std::string(rootSystemPath) + "]").c_str());
    
    // Check if the HTML file exists
    std::string htmlFilePath = rootSystemPath;
    if (htmlFilePath[0] == '/') {
        htmlFilePath = htmlFilePath.substr(1); // Remove leading slash for LittleFS
    }
    htmlFilePath += "/SPAmanager.html";
    
    if (!filesystemAvailable || !LittleFS.exists(htmlFilePath.c_str())) {
        debug("Using minimal HTML fallback");
        return MINIMAL_HTML;
    }
    
    return R"HTML(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Display Manager</title>
    <link rel="stylesheet" href="/SPAmanager.css">
</head>
<body>
    <script>
        window.location.href = "/SPAmanager.html";
    </script>
</body>
</html>
)HTML";
}



std::string SPAmanager::generateMenuHTML()
{
  debug("generateMenuHTML() called");
  std::string menuHTML = "";
  
  for (const auto& menu : menus)
  {
    if (activePage && strcmp(menu.pageName, activePage->name) == 0)
    {
      menuHTML += "<div class=\"dM_dropdown\"><span>" + std::string(menu.name) + "</span><ul class=\"dM_dropdown-menu\">";
      
      for (const auto& item : menu.items)
      {
        menuHTML += "<li" + std::string(item.disabled ? " class=\"disabled\"" : "") + ">";
        
        if (item.hasUrl())
        {
          menuHTML += "<a href=\"" + std::string(item.url) + "\">" + std::string(item.name) + "</a>";
        }
        else
        {
          menuHTML += "<span data-menu=\"" + std::string(menu.name) + "\" data-item=\"" + std::string(item.name) + "\"";
          if (!item.disabled)
          {
            menuHTML += " onclick=\"handleMenuClick('" + std::string(menu.name) + "', '" + std::string(item.name) + "')\"";
          }
          menuHTML += ">" + std::string(item.name) + "</span>";
        }
        
        menuHTML += "</li>";
      }
      
      menuHTML += "</ul></div>";
    }
  }
  
  return menuHTML;
}


std::string SPAmanager::getSystemFilePath() const
{
  return rootSystemPath;
}
