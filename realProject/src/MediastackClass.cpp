#include "MediastackClass.h"

Mediastack::Mediastack(WiFiClient &thisClient) : thisClient(thisClient), apiUrl("") {}

void Mediastack::setup(const char *key, uint8_t newRequestInterval, uint8_t newMaxMessages, bool newOnlyDuringDay, Stream* debugPort)
{
  debug = debugPort;
  char tmp[100];
  apiKey = key;
  requestInterval = newRequestInterval;
  onlyDuringDay = newOnlyDuringDay;
  maxMessages = newMaxMessages;
  if (debug) debug->flush();
  if (debug) debug->printf("\nMediastack::setup() with apiKey[%s]\n", apiKey.c_str());
  if (debug) debug->printf("Mediastack::setup() with requestInterval[%d minutes]\n", requestInterval);
  if (debug) debug->printf("Mediastack::setup() with maxMessages[%d]\n", maxMessages);
  if (debug) debug->printf("Mediastack::setup() with onlyDuringDay[%s]\n", onlyDuringDay ? "true" : "false");
  debug->printf("Mediastack::setup() Done! Free Heap: [%d] bytes\n", ESP.getFreeHeap());

} //  setup()

void Mediastack::setInterval(int newInterval)
{
  requestInterval = newInterval;

} //  setInterval()

bool Mediastack::loop(struct tm timeInfo)
{
  if (millis() > loopTimer && apiKey.length() > 5 && maxMessages > 0)
  {
    bool isDaytime = true;
    if (onlyDuringDay)
    {
      // Define daytime as between 6:00 AM and 10:00 PM
      int hour = timeInfo.tm_hour;
      bool isDaytime = (hour >= 8 && hour < 18);
      
      if (!isDaytime)
      {
        if (debug) debug->println("Mediastack::loop() skipping - not daytime");
        loopTimer = millis() + (requestInterval * (60 * 500)); // halve Interval in Minutes!
        return false;
      }
    }
  
    loopTimer = millis() + (requestInterval * (60 * 1000)); // Interval in Minutes!
    bool result = fetchNews();
    if (debug) debug->printf("Mediastack::loop() fetchNews->result[%s]\n", result ? "true" : "false");
    return result;
  }
  return false;

} // loop()

void Mediastack::replaceUnicode(char* newsMessage) {
    const char* unicodeSequences[] = { "\\u2019", "\\u201e", "\\u201d", "\\u00e9", "\\u2018" };
    const char replacements[] = { '"', '"', '"', 'e', '"' };

    char* pos = newsMessage;
    while (*pos != '\0') {
        bool replaced = false;
        for (size_t i = 0; i < sizeof(unicodeSequences) / sizeof(unicodeSequences[0]); ++i) {
            size_t seqLength = strlen(unicodeSequences[i]);
            if (strncmp(pos, unicodeSequences[i], seqLength) == 0) {
                *pos = replacements[i];
                memmove(pos + 1, pos + seqLength, strlen(pos + seqLength) + 1);
                replaced = true;
                break;
            }
        }

        if (!replaced && strncmp(pos, "\\u", 2) == 0 &&
            isxdigit(pos[2]) && isxdigit(pos[3]) && isxdigit(pos[4]) && isxdigit(pos[5])) {
            *pos = '?';
            memmove(pos + 1, pos + 6, strlen(pos + 6) + 1);
        }

        if (!replaced) ++pos;
    }
}

bool Mediastack::fetchNews() 
{
    // Create a local WiFiClient that will be automatically destroyed when the function exits
    WiFiClient localClient;
    
    const char* host = "api.mediastack.com";
    const int port = 80;
    char newsMessage[NEWS_SIZE] = {};
    int statusCode = 0;
    bool success = false;

    debug->printf("Connecting to %s:%d\n", host, port);
    if (!localClient.connect(host, port)) {
        debug->println("Connection failed.");
        for (int i = 0; i <= maxMessages; ++i) {
            writeFileById("NWS", i, (i == 1) ? "There is No News ...." : "");
        }
        return false;
    }

    // Send the HTTP request
    localClient.print(String("GET ") + "/v1/news?access_key=" + apiKey + "&countries=nl" + " HTTP/1.0\r\n" +
                 "Host: " + host + "\r\n" +
                 "User-Agent: ESP-ticker\r\n" +
                 "Connection: close\r\n\r\n");

    // Set a timeout for the entire operation
    unsigned long totalTimeout = millis() + 30000; // 30 second total timeout
    
    // Wait for response with timeout
    while (!localClient.available() && millis() < totalTimeout) {
        yield();
    }
    
    if (millis() >= totalTimeout) {
        debug->println("Total operation timeout.");
        // Don't call stop() here, let the destructor handle it
        for (int i = 0; i <= maxMessages; ++i) {
            writeFileById("NWS", i, (i == 1) ? "Operation timeout ...." : "");
        }
        return false;
    }

    // Process the HTTP response
    if (localClient.find("HTTP/1.1")) {
        statusCode = localClient.parseInt();
        debug->printf("HTTP Status: %d\n", statusCode);
        
        if (statusCode != 200) {
            for (int i = 0; i <= maxMessages; ++i) {
                if (i == 1 && statusCode == 429)
                    writeFileById("NWS", i, "Too many requests. Try again later.");
                else
                    writeFileById("NWS", i, (i == 1) ? "Er is geen nieuws ...." : "");
            }
            return false;
        }
        
        // Skip headers
        if (!localClient.find("\r\n\r\n")) {
            debug->println("Could not find end of headers.");
            for (int i = 0; i <= maxMessages; ++i) {
                writeFileById("NWS", i, (i == 1) ? "Er is geen nieuws ...." : "");
            }
            return false;
        }
        
        // Process news items
        int msgNr = 0;
        
        // Read and process news items with timeout
        while (localClient.available() && millis() < totalTimeout) {
            yield();
            
            if (localClient.find("\"title\":\"")) {
                memset(newsMessage, 0, sizeof(newsMessage));
                int msgIdx = 0;
                
                // Read until description or buffer limit
                while (strIndex(newsMessage, "\"description\":") == -1 && 
                       msgIdx < NEWS_SIZE - 2 && 
                       localClient.available()) {
                    char c = localClient.read();
                    if (c >= 32 && c <= 126)
                        newsMessage[msgIdx++] = c;
                    yield();
                }
                
                newsMessage[msgIdx] = '\0';
                
                // Remove description part
                if (msgIdx > 30)
                    newsMessage[msgIdx - 16] = 0;
                
                replaceUnicode(newsMessage);
                
                if (!hasNoNoWord(newsMessage) && strlen(newsMessage) > 15) {
                    if (msgNr <= maxMessages) {
                        writeFileById("NWS", msgNr, newsMessage);
                    }
                    msgNr++;
                }
            }
        }
        
        // Fill empty slots or clear remaining slots
        if (msgNr == 0) {
            for (int i = 0; i <= maxMessages; ++i) {
                writeFileById("NWS", i, (i == 1) ? "Er is geen nieuws ...." : "");
            }
        } else {
            for (int i = msgNr; i <= maxMessages; ++i) {
                writeFileById("NWS", i, "");
            }
            success = true;
        }
    } else {
        debug->println("Malformed response.");
        for (int i = 0; i <= maxMessages; ++i) {
            writeFileById("NWS", i, (i == 1) ? "Er is geen nieuws ...." : "");
        }
    }
    
    // IMPORTANT: Don't explicitly call stop() - let the destructor handle it
    debug->println("Fetch news completed, returning from function");
    
    // Final message if successful
    if (success) {
        updateMessage("0", "News brought to you by 'Mediastack.com'");
    }
    
    return success;

} // fetchNews()


void Mediastack::clearNewsFiles()
{
  char path[32];
  for (int i = 0; i <= maxMessages; ++i)
  {
    snprintf(path, sizeof(path), "/Mediastack/NWS-%03d.txt", i);
    debug->printf("Deleting: %s\n", path);
    
    if (LittleFS.exists(path))
    {
      if (LittleFS.remove(path))
      {
        if (debug) debug->printf("Successfully deleted: %s\n", path);
      }
      else
      {
        if (debug) debug->printf("Failed to delete: %s\n", path);
      }
    }
    else
    {
      if (debug) debug->printf("File does not exist: %s\n", path);
    }
  }

} // clearNewsFiles()

// ========== Default Implementations (can be overridden) ==========

void Mediastack::updateMessage(const char* id, const char* msg) {
    debug->printf("[updateMessage] id: %s | msg: %s\n", id, msg);
}

void Mediastack::writeFileById(const char* prefix, int id, const char* content) 
{
  char path[32];

  debug->printf("Mediastack::writeFileById():  %s-%03d.txt -> %s\n", prefix, id, content);
  // First, ensure the Mediastack directory exists
  if (!LittleFS.exists("/Mediastack")) 
  {
    if (debug) debug->println("Mediastack::writeFileById(): Creating [/Mediastack] directory");
    if (!LittleFS.mkdir("/Mediastack")) 
    {
      if (debug) debug->println("Mediastack::writeFileById(): Failed to create [/Mediastack] directory");
      return;
    }
  }
  
  snprintf(path, sizeof(path), "/Mediastack/%s-%03d.txt", prefix, id);
  
  File file = LittleFS.open(path, "w");
  if (!file)
  {
    if (debug) debug->printf("Mediastack::writeFileById(): Failed to open file for writing: %s\n", path);
    return;
  }
  
  if (file.print(content))
  {
    if (debug) debug->printf("Mediastack::writeFileById(): File written successfully: %s\n", path);
  }
  else
  {
    if (debug) debug->printf("Mediastack::writeFileById(): Write failed for file: %s\n", path);
  }
  
  file.close();

} // writeFileById()


std::string Mediastack::readFileById(const char* prefix, int id) 
{
  char path[32];
  snprintf(path, sizeof(path), "/Mediastack/%s-%03d.txt", prefix, id);
  if (debug) debug->printf("Mediastack::readFileById(): readFileById: %s\n", path);

  File file = LittleFS.open(path, "r");
  if (!file)
  {
    if (debug) debug->printf("Mediastack::readFileById(): Failed to open file for reading: %s\n", path);
    return "";
  }

  std::string content;
  while (file.available()) {
    content += (char)file.read();
  }

  file.close();
  return content;

} //  readFileById()


bool Mediastack::hasNoNoWord(const char* text) {
    return false;
}

int Mediastack::strIndex(const char* str, const char* substr) {
    const char* found = strstr(str, substr);
    return found ? (int)(found - str) : -1;
}