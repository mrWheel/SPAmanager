//----- SPAmanager.cpp -----
#include "SPAmanager.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <sstream>
#include <WString.h>

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


    // Store in rootSystemPath (assuming rootSystemPath is std::string)
    rootSystemPath = path;

    this->debugOut = debugOut;

    if (!LittleFS.begin(true)) 
    {
        debug("An error occurred while mounting LittleFS");
        return;
    }

    debug(("begin(): called with rootSystemPath: [" + rootSystemPath + "]").c_str());
    setupWebServer();

} //  begin()

void SPAmanager::setupWebServer() 
{
    debug("setupWebServer() called");
    ws.begin();
    ws.onEvent([this](uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
        handleWebSocketEvent(num, type, payload, length);
    });

    std::string filePath = std::string(rootSystemPath) + "/SPAmanager.css";
    server.serveStatic("/SPAmanager.css", LittleFS, filePath.c_str());
    debug(("server.serveStatic(/SPAmanager.css, LittleFS, " + filePath +")").c_str());
    filePath = std::string(rootSystemPath) + "/SPAmanager.html";
    server.serveStatic("/SPAmanager.html", LittleFS, filePath.c_str());
    debug(("server.serveStatic(/SPAmanager.html, LittleFS, " + filePath +")").c_str());
    filePath = std::string(rootSystemPath) + "/SPAmanager.js";
    server.serveStatic("/SPAmanager.js", LittleFS, filePath.c_str());
    debug(("server.serveStatic(/SPAmanager.js, LittleFS, " + filePath +")").c_str());
    filePath = std::string(rootSystemPath) + "/disconnected.html";
    server.serveStatic("/disconnected.html", LittleFS, filePath.c_str());
    debug(("server.serveStatic(/disconnected.html, LittleFS, " + filePath).c_str());

    server.on("/", HTTP_GET, [this]() {
        server.send(200, "text/html", generateHTML().c_str());
    });
    server.begin();
}

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
        
        DeserializationError error = deserializeJson(doc, message);
        
        if (error)
        {

            debug(("JSON deserialization failed: " + std::string(error.c_str())).c_str());
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
                        std::string content = page.getContent();
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
                                page.setContent(content.c_str());
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
    const size_t capacity = JSON_ARRAY_SIZE(5) +
                           JSON_ARRAY_SIZE(10) +
                           10 * JSON_OBJECT_SIZE(3) +
                           JSON_OBJECT_SIZE(10) +
                           1024;
                           
    DynamicJsonDocument doc(capacity);
    if (doc.capacity() == 0)
    {
        debug("Failed to allocate JSON buffer for broadcast state");
        return;
    }
    
    doc["body"] = activePage ? activePage->getContent() : "";
    doc["pageName"] = activePage ? activePage->name : "";
    doc["isVisible"] = activePage ? true : false;
    
    doc["message"] = currentMessage;
    doc["isError"] = isError;
    doc["messageDuration"] = messageEndTime > 0 ? (messageEndTime - millis()) : 0;
    
    JsonArray menuArray = doc.createNestedArray("menus");
    for (const auto& menu : menus) 
    {
        if (strcmp(menu.pageName, activePage ? activePage->name : "") == 0) 
        {
            JsonObject menuObj = menuArray.createNestedObject();
            menuObj["name"] = menu.name;
            JsonArray itemArray = menuObj.createNestedArray("items");
            
            for (const auto& item : menu.items)
            {
                JsonObject itemObj = itemArray.createNestedObject();
                itemObj["name"] = item.name;
                if (item.hasUrl()) 
                {
                    itemObj["url"] = item.url;
                }
                itemObj["disabled"] = item.disabled;
            }
        }
    }
    
        std::string output;
        serializeJson(doc, output);
        
        if (!output.empty()) 
        {
            ws.broadcastTXT(output.c_str(), output.length());
        } 
    else 
    {
        debug("Failed to serialize JSON for broadcast state");
    }
}

void SPAmanager::addPage(const char* pageName, const char* html) 
{
    debug(("addPage() called with pageName: " + std::string(pageName)).c_str());
    
    if (pages.empty()) {
      firstPageName = pageName;  // Store the first page name
    }
    
    auto it = std::find_if(pages.begin(), pages.end(),
        [pageName](const Page& page) { return strcmp(page.name, pageName) == 0; });
    
    if (it != pages.end()) 
    {
        it->setContent(html);
        updateClients();
    }
    else 
    {
        Page page;
        page.setName(pageName);
        page.setContent(html);
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
}


template <typename T>
void SPAmanager::setPlaceholder(const char* pageName, const char* placeholder, T value) {
    //debug((std::string("setPlaceholder() called with pageName: ") + pageName + ", placeholder: " + placeholder + ", value: " + std::to_string(value)).c_str());
    for (auto& page : pages) {
        if (strcmp(page.name, pageName) == 0) {
            std::string content = page.getContent();
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
                page.setContent(content.c_str());
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
            }
            break;
        }
    }
}

// Explicit specialization for char* to handle string literals
template <>
void SPAmanager::setPlaceholder<const char*>(const char* pageName, const char* placeholder, const char* value) {
    debug((std::string("setPlaceholder() called with pageName: ") + pageName + ", placeholder: " + placeholder + ", value: " + value).c_str());
    for (auto& page : pages) {
        if (strcmp(page.name, pageName) == 0) {
            std::string content = page.getContent();
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
                page.setContent(content.c_str());
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
            }
            break;
        }
    }
}

SPAmanager::PlaceholderValue SPAmanager::getPlaceholder(const char* pageName, const char* placeholder)
{
  debug((std::string("getPlaceholder() called with pageName: ") + pageName + ", placeholder: " + placeholder).c_str());
  std::string value = "";
  
  for (const auto& page : pages)
  {
    if (strcmp(page.name, pageName) == 0)
    {
      std::string content = page.getContent();
      std::string idStr1 = std::string("id='") + placeholder + "'";
      std::string idStr2 = std::string("id=\"") + placeholder + "\"";
      size_t pos = content.find(idStr1);
      if (pos == std::string::npos) {
          pos = content.find(idStr2);
      }
      
      if (pos != std::string::npos)
      {
        // Check if it's an input field
        size_t inputStart = content.rfind("<input", pos);
        if (inputStart != std::string::npos && inputStart < pos)
        {
          // Find value attribute with single or double quotes
          size_t valueStart1 = content.find("value='", inputStart);
          size_t valueStart2 = content.find("value=\"", inputStart);
          size_t closingBracket = content.find(">", inputStart);
          
          if (valueStart1 != std::string::npos && valueStart1 < closingBracket)
          {
            valueStart1 += 7; // Length of "value='"
            size_t valueEnd = content.find("'", valueStart1);
            if (valueEnd != std::string::npos)
            {
              value = content.substr(valueStart1, valueEnd - valueStart1);
            }
          }
          else if (valueStart2 != std::string::npos && valueStart2 < closingBracket)
          {
            valueStart2 += 7; // Length of "value=\""
            size_t valueEnd = content.find("\"", valueStart2);
            if (valueEnd != std::string::npos)
            {
              value = content.substr(valueStart2, valueEnd - valueStart2);
            }
          }
        }
        else
        {
          // For non-input elements, get content between tags
          size_t start = content.find('>', pos) + 1;
          size_t end = content.find('<', start);
          
          if (start != std::string::npos && end != std::string::npos)
          {
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
}

// Explicit template instantiations for setPlaceholder
template void SPAmanager::setPlaceholder<unsigned int>(const char*, const char*, unsigned int);
template void SPAmanager::setPlaceholder<int>(const char*, const char*, int);
template void SPAmanager::setPlaceholder<float>(const char*, const char*, float);
template void SPAmanager::setPlaceholder<double>(const char*, const char*, double);

void SPAmanager::activatePage(const char* pageName) 
{
    debug(("activatePage() called with pageName: " + std::string(pageName)).c_str());
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
      std::string content = page.getContent();
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
        
        page.setContent(content.c_str());
        if (activePage && strcmp(activePage->name, pageName) == 0)
        {
          updateClients();
        }
      }
      break;
    }
  }
}

void SPAmanager::disableID(const char* pageName, const char* id)
{
  debug(("disableID() called with pageName: " + std::string(pageName) + ", id: " + std::string(id)).c_str());
  for (auto& page : pages)
  {
    if (strcmp(page.name, pageName) == 0)
    {
      std::string content = page.getContent();
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
        
        page.setContent(content.c_str());
        if (activePage && strcmp(activePage->name, pageName) == 0)
        {
          updateClients();
        }
      }
      break;
    }
  }
}


void SPAmanager::includeJsFile(const std::string &path2JsFile)
{
  std::string sanitizedJsPath = path2JsFile;
  
  debug(("SPAmanager::includeJsFile() called with path2JsFile: [" + sanitizedJsPath + "]").c_str());

  //-- Reject a single slash as an invalid path
  if (sanitizedJsPath == "/") 
  {
    debug("ERROR: path2JsFile cannot be '/'");
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
    debug(("Script [" + sanitizedJsPath + "] already served, skipping").c_str());
    return;
  }

  debug(("Adding script to servedFiles: [" + sanitizedJsPath + "]").c_str());

  //-- Extract `fName` (last part of the path) for `serveStatic()`
  std::string fName = sanitizedJsPath.substr(sanitizedJsPath.find_last_of('/') + 1);
  if (!fName.empty() && fName[0] != '/')
  {
    fName.insert(0, 1, '/');
  }

  //-- Debugging `serveStatic()` call
  debug(("server.serveStatic(" + fName + ", LittleFS, " + sanitizedJsPath + ")").c_str());

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
    debug("ERROR: path2CssFile cannot be '/'");
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
    debug(("CSS [" + sanitizedCssPath + "] already served, skipping").c_str());
    return;
  }

  debug(("Adding CSS to servedFiles: [" + sanitizedCssPath + "]").c_str());

  //-- Extract `fName` (last part of the path) for `serveStatic()`
  std::string fName = sanitizedCssPath.substr(sanitizedCssPath.find_last_of('/') + 1);
  if (!fName.empty() && fName[0] != '/')
  {
    fName.insert(0, 1, '/');
  }

  //-- Debugging `serveStatic()` call
  debug(("server.serveStatic(" + fName + ", LittleFS, " + sanitizedCssPath + ")").c_str());

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
  if (success)
  {
    debug(("JavaScript function [" + std::string(functionName) + "] executed successfully").c_str());
  }
  else
  {
    debug(("JavaScript function [" + std::string(functionName) + "] not found or failed to execute").c_str());
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

void SPAmanager::updateClients() 
{
    if (messageEndTime > 0 && millis() >= messageEndTime) 
    {
        currentMessage[0] = '\0';
        messageEndTime = 0;
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


std::string SPAmanager::generateHTML()
{
  debug(("generateHTML() called (systemFiles are in [" + std::string(rootSystemPath) + "]").c_str());
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
