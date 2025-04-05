//----- SPAmanager.h -----
#ifndef SPA_MANAGER_H
#define SPA_MANAGER_H

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <functional>
#include <vector>
#include <string.h>
#include <stdlib.h>
#include <set>
#include <map>

class SPAmanager 
{
  private:
    static const size_t MAX_NAME_LEN = 32;
    static const size_t MAX_URL_LEN = 64;
    static const size_t MAX_CONTENT_LEN = 4096;
    static const size_t MAX_MESSAGE_LEN = 80;
    static const size_t MAX_VALUE_LEN = 32;

  public:
    WebServer server;
    WebSocketsServer ws;
    
    SPAmanager(uint16_t port = 80);
    void begin(const char* systemPath, Stream* debugOut = nullptr);
    
    //-- Page-related methods
    void addPage(const char* pageName, const char* html);
    void activatePage(const char* pageName);
    std::string getActivePageName() const;
    void setPageTitle(const char* pageName, const char* title);
    
    //-- Menu-related methods
    void addMenu(const char* pageName, const char* menuName);
    void addMenuItem(const char* pageName, const char* menuName, const char* itemName, std::function<void()> callback);
    void addMenuItem(const char* pageName, const char* menuName, const char* itemName, const char* url);
    void addMenuItem(const char* pageName, const char* menuName, const char* itemName, std::function<void(const char*)> callback, const char* param);
    void addMenuItemPopup(const char* pageName, const char* menuName, const char* menuItem, const char* popupMenu, std::function<void(const std::map<std::string, std::string>&)> callback = nullptr);
    void enableMenuItem(const char* pageName, const char* menuName, const char* itemName);
    void disableMenuItem(const char* pageName, const char* menuName, const char* itemName);
    
    //-- UI/Interaction methods
    void setMessage(const char* message, int duration);
    void setErrorMessage(const char* message, int duration);
    void callJsFunction(const char* functionName);
    void addWebSocketEvent(WStype_t eventType, std::function<void(uint8_t num, uint8_t* payload, size_t length)> callback);
    template <typename T>
    void setPlaceholder(const char* pageName, const char* placeholder, T value);
    class PlaceholderValue 
    {
      public:
        PlaceholderValue(const char* v) {
            strncpy(value, v, MAX_VALUE_LEN-1);
            value[MAX_VALUE_LEN-1] = '\0';
        }
        
        int asInt() const { return atoi(value); }
        float asFloat() const { return atof(value); }
        const char* c_str() const { return value; }

      private:
        char value[MAX_VALUE_LEN];
      
    };
    PlaceholderValue getPlaceholder(const char* pageName, const char* placeholder);
    void enableID(const char* pageName, const char* id);
    void disableID(const char* pageName, const char* id);
    void pageIsLoaded(std::function<void()> callback);
    
    // Resource methods
    std::string getSystemFilePath() const;
    void includeCssFile(const std::string &cssFile);
    void includeJsFile(const std::string &scriptFile);

  private:
    Stream* debugOut;
    uint8_t currentClient;  //-- Store current connected client number
    bool hasConnectedClient;  //-- Track if we have a connected client
    std::string rootSystemPath;
    std::string firstPageName;  //-- Store the name of the first page added
    std::string activePageName; //-- Store the name of the active page
    std::function<void()> pageLoadedCallback;
    
    char currentMessage[MAX_MESSAGE_LEN];
    bool isError;
    unsigned long messageEndTime;
    
    struct MenuItem 
    {
        char name[MAX_NAME_LEN];
        char url[MAX_URL_LEN];
        std::function<void()> callback;
        std::function<void(const std::map<std::string, std::string>&)> popupCallback; // Modified to accept input values
        bool disabled = false;
        
        void setName(const char* n) {
            strncpy(name, n, MAX_NAME_LEN-1);
            name[MAX_NAME_LEN-1] = '\0';
        }
        
        void setUrl(const char* u) {
            if (u) {
                strncpy(url, u, MAX_URL_LEN-1);
                url[MAX_URL_LEN-1] = '\0';
            } else {
                url[0] = '\0';
            }
        }
        
        bool hasUrl() const {
            return url[0] != '\0';
        }
    };

    struct Menu 
    {
      char name[MAX_NAME_LEN];
      char pageName[MAX_NAME_LEN];
      std::vector<MenuItem> items;
      
      void setName(const char* n) {
          strncpy(name, n, MAX_NAME_LEN-1);
          name[MAX_NAME_LEN-1] = '\0';
      }
      
      void setPageName(const char* n) {
          strncpy(pageName, n, MAX_NAME_LEN-1);
          pageName[MAX_NAME_LEN-1] = '\0';
      }
    };

    struct Page 
    {
      char name[MAX_NAME_LEN];
      char title[MAX_NAME_LEN];
      char content[MAX_CONTENT_LEN];
      bool isVisible;
      
      void setName(const char* n) {
          strncpy(name, n, MAX_NAME_LEN-1);
          name[MAX_NAME_LEN-1] = '\0';
          strncpy(title, n, MAX_NAME_LEN-1);  // Default title to page name
          title[MAX_NAME_LEN-1] = '\0';
      }
      
      void setTitle(const char* t) {
          strncpy(title, t, MAX_NAME_LEN-1);
          title[MAX_NAME_LEN-1] = '\0';
      }
      
      void setContent(const char* c) {
          strncpy(content, c, MAX_CONTENT_LEN-1);
          content[MAX_CONTENT_LEN-1] = '\0';
      }
      
      const char* getContent() const {
          return content;
      }
    };

    std::vector<Menu> menus;
    std::vector<Page> pages;
    Page* activePage;
    //-- store eventListeners for WebSocket events
    std::map<WStype_t, std::vector<std::function<void(uint8_t num, uint8_t* payload, size_t length)>>> wsEventListeners;
    //-- Track which scripts have been served to avoid duplicates
    std::set<std::string> servedFiles;  
    
    //-- Server setup and handling
    void setupWebServer();
    void handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
    
    //-- UI generation
    std::string generateHTML();
    std::string generateMenuHTML();
    void setHeaderTitle(const char* title);
    
    //-- State management
    void broadcastState();
    void updateClients();
    
    //-- Utility methods
    void debug(const char* message);
    void handleJsFunctionResult(const char* functionName, bool success);
};

#endif // SPA_MANAGER_H
