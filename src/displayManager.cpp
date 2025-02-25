// displayManager.cpp
#include "displayManager.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <sstream>
#include <WString.h>

DisplayManager::DisplayManager(uint16_t port) 
    : server(port)
    , ws(81)
    , debugOut(nullptr)
    , currentClient(0)
    , hasConnectedClient(false)
    , currentMessage{0}
    , isError(false)
    , messageEndTime(0)
    , menus()
    , pages()
    , activePage(nullptr)
    , servedScripts()  // Initialize empty set

{
    debug(("DisplayManager constructor called with port: " + std::to_string(port)).c_str());
}

void DisplayManager::begin(Stream* debugOut) 
{
    debug("begin() called");
    this->debugOut = debugOut;
    if (!LittleFS.begin(true)) 
    {
        debug("An error occurred while mounting LittleFS");
        return;
    }
    setupWebServer();
}

void DisplayManager::setupWebServer() 
{
    debug("setupWebServer() called");
    ws.begin();
    ws.onEvent([this](uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
        handleWebSocketEvent(num, type, payload, length);
    });
    
    server.serveStatic("/displayManager.css", LittleFS, "/displayManager.css");
    server.serveStatic("/displayManager.html", LittleFS, "/displayManager.html");
    server.serveStatic("/disconnected.html", LittleFS, "/disconnected.html");
  //server.serveStatic("/fsManager.js", LittleFS, "/fsManager.js");
    server.on("/", HTTP_GET, [this]() {
        server.send(200, "text/html", generateHTML().c_str());
    });
    server.begin();
}


void DisplayManager::includeJsScript(const char* scriptFile)
{
  debug(("DisplayManager::includeJsScript() called with scriptFile: [" + std::string(scriptFile) + "]").c_str());
  if (hasConnectedClient)
  {
    // Check if script has already been served
    std::string scriptPath(scriptFile);
    if (servedScripts.find(scriptPath) != servedScripts.end()) {
      debug(("Script [" + scriptPath + "] already served, skipping").c_str());
      return;
    }

    const size_t capacity = JSON_OBJECT_SIZE(2);
    DynamicJsonDocument doc(capacity);

    doc["event"] = "includeJsScript";
    doc["data"]  = scriptFile;

    std::string output;
    serializeJson(doc, output);

    if (!output.empty())
    {
      server.serveStatic(scriptFile, LittleFS, scriptFile);
      ws.broadcastTXT(output.c_str(), output.length());
      servedScripts.insert(scriptPath);  // Mark script as served
    }
  }
}

void DisplayManager::callJsFunction(const char* functionName)
{
  debug(("DisplayManager::callJsFunction() called with function: " + std::string(functionName)).c_str());
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
}

void DisplayManager::handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) 
{
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
    }
    else if (type == WStype_DISCONNECTED)
    {
        // Only clear connection if it's our current client
        if (num == currentClient)
        {
            hasConnectedClient = false;
            debug("Current client disconnected");
        }
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
        const size_t capacity = JSON_OBJECT_SIZE(3) + 256;
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
        }
        else if (doc["type"] == "inputChange") {
            const char* placeholder = doc["placeholder"];
            const char* value = doc["value"];
            
            if (activePage) {
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
        }
    }
}

void DisplayManager::broadcastState() 
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

void DisplayManager::addPage(const char* pageName, const char* html) 
{
    debug(("addPage() called with pageName: " + std::string(pageName)).c_str());
    
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

template <typename T>
void DisplayManager::setPlaceholder(const char* pageName, const char* placeholder, T value) {
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
void DisplayManager::setPlaceholder<const char*>(const char* pageName, const char* placeholder, const char* value) {
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

void DisplayManager::activatePage(const char* pageName) 
{
    debug(("activatePage() called with pageName: " + std::string(pageName)).c_str());
    for (auto& page : pages) 
    {
        bool shouldBeVisible = (strcmp(page.name, pageName) == 0);
        if (shouldBeVisible) 
        {
            activePage = &page;
            setHeaderTitle(page.title);
            Serial.printf("Activating page: %s with [%s]\n", page.name, page.title);
        }
        page.isVisible = shouldBeVisible;
    }
    updateClients();
}

void DisplayManager::addMenu(const char* pageName, const char* menuName) 
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

void DisplayManager::addMenuItem(const char* pageName, const char* menuName, const char* itemName, std::function<void()> callback) 
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

void DisplayManager::addMenuItem(const char* pageName, const char* menuName, const char* itemName, const char* url) 
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

void DisplayManager::addMenuItem(const char* pageName, const char* menuName, const char* itemName, std::function<void(uint8_t)> callback, uint8_t param) 
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

void DisplayManager::setMessage(const char* message, int duration) 
{
    debug(("setMessage() called with message: " + std::string(message) + ", duration: " + std::to_string(duration)).c_str());
    strncpy(currentMessage, message, MAX_MESSAGE_LEN-1);
    currentMessage[MAX_MESSAGE_LEN-1] = '\0';
    isError = false;
    messageEndTime = duration > 0 ? millis() + (duration * 1000) : 0;
    updateClients();
}

void DisplayManager::setErrorMessage(const char* message, int duration) 
{
    debug(("setErrorMessage() called with message: " + std::string(message) + ", duration: " + std::to_string(duration)).c_str());
    strncpy(currentMessage, message, MAX_MESSAGE_LEN-1);
    currentMessage[MAX_MESSAGE_LEN-1] = '\0';
    isError = true;
    messageEndTime = duration > 0 ? millis() + (duration * 1000) : 0;
    updateClients();
}

void DisplayManager::enableMenuItem(const char* pageName, const char* menuName, const char* itemName)
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

void DisplayManager::disableMenuItem(const char* pageName, const char* menuName, const char* itemName)
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

void DisplayManager::updateClients() 
{
    if (messageEndTime > 0 && millis() >= messageEndTime) 
    {
        currentMessage[0] = '\0';
        messageEndTime = 0;
    }
    broadcastState();
}

void DisplayManager::debug(const char* message) 
{
    if (debugOut) 
    {
        debugOut->println(message);
    }
}

DisplayManager::PlaceholderValue DisplayManager::getPlaceholder(const char* pageName, const char* placeholder)
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
template void DisplayManager::setPlaceholder<unsigned int>(const char*, const char*, unsigned int);
template void DisplayManager::setPlaceholder<int>(const char*, const char*, int);
template void DisplayManager::setPlaceholder<float>(const char*, const char*, float);
template void DisplayManager::setPlaceholder<double>(const char*, const char*, double);

void DisplayManager::setHeaderTitle(const char* title)
{
  debug(("setHeaderTitle() called with title: " + std::string(title)).c_str());
  
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

void DisplayManager::setPageTitle(const char* pageName, const char* title)
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

void DisplayManager::enableID(const char* pageName, const char* id)
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

void DisplayManager::disableID(const char* pageName, const char* id)
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

std::string DisplayManager::generateHTML() 
{
    debug("generateHTML() called");
    
    File file = LittleFS.open("/displayManager.html", "r");
    if (!file)
    {
        debug("Failed to open displayManager.html");
        return "";
    }
    
    std::string html = file.readString().c_str();
    file.close();
    
    // Find the bodyContent div and insert the page content
    const char* pageContent = activePage ? activePage->getContent() : "";
    size_t pos = html.find("<div id=\"bodyContent\"");
    if (pos != std::string::npos)
    {
        pos = html.find(">", pos) + 1;
        size_t endPos = html.find("</div>", pos);
        if (pos != std::string::npos && endPos != std::string::npos)
        {
            html.replace(pos, endPos - pos, pageContent);
        }
    }
    
    return html;
}

std::string DisplayManager::generateMenuHTML() 
{
    debug("generateMenuHTML() called");
    std::string html;
    html.reserve(4096);
    
    if (!activePage) 
    {
        debug("No active page, skipping menu generation");
        return html;
    }

    debug(("Generating menus for page: " + std::string(activePage->name)).c_str());
    
    bool hasMenus = false;
    for (const auto& menu : menus) 
    {
        if (strcmp(menu.pageName, activePage->name) == 0)
        {
            hasMenus = true;
            debug(("Adding menu: " + std::string(menu.name)).c_str());
            
            html += "<div class='dM_dropdown'><span>";
            html += menu.name;
            html += "</span><ul class='dM_dropdown-menu'>";
    
            for (const auto& item : menu.items) 
            {
                html += "<li";
                if (item.disabled) 
                {
                    html += " class='disabled'";
                }
                html += ">";
                
                if (item.hasUrl()) 
                {
                    html += "<a data-menu='";
                    html += menu.name;
                    html += "' data-item='";
                    html += item.name;
                    html += "' href='";
                    html += item.url;
                    html += "'>";
                    html += item.name;
                    html += "</a>";
                } 
                else 
                {
                    html += "<span data-menu='";
                    html += menu.name;
                    html += "' data-item='";
                    html += item.name;
                    html += "'";
                    if (!item.disabled) {
                        html += " onclick='handleMenuClick(\"";
                        html += menu.name;
                        html += "\", \"";
                        html += item.name;
                        html += "\")'";
                    }
                    html += ">";
                    html += item.name;
                    html += "</span>";
                }
                html += "</li>";
            }
            html += "</ul></div>";
        }
    }
    
    if (!hasMenus) {
        debug(("No menus found for active page: " + std::string(activePage->name)).c_str());
    }
    
    return html;
}

void DisplayManager::pageIsLoaded(std::function<void()> callback)
{
  debug("pageIsLoaded(): called");
  servedScripts.clear();
  pageLoadedCallback = callback;
}