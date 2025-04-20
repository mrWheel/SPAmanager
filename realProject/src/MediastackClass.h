#ifndef MEDIASTACK_CLASS_H
#define MEDIASTACK_CLASS_H

#include <Arduino.h>
#include <WiFiClient.h>
#include <LittleFS.h>

#define NEWS_SIZE 512

class Mediastack {
public:
    Mediastack(WiFiClient &wmediastackClient);
    void setup(const char *key, uint8_t newRequestInterval, uint8_t newMaxMessages, bool newOnlyDuringDay, Stream* debugPort);
    void setInterval(int newInterval);
    bool loop(struct tm timeInfo);
    bool fetchNews();
    void clearNewsFiles();

    void updateMessage(const char* id, const char* msg);
    void writeFileById(const char* prefix, int id, const char* content);
    std::string readFileById(const char* prefix, int id);
    bool hasNoNoWord(const char* text);
    int strIndex(const char* str, const char* substr);

private:
    WiFiClient         &thisClient;
    void replaceUnicode(char* message);

    uint8_t            requestInterval = 0;
    uint32_t            loopTimer = 0;
    uint8_t             maxMessages;
    bool                onlyDuringDay;  
    String              apiUrl;
    static const char  *apiHost;
    String              apiKey;
    Stream* debug = nullptr; // Optional, default to nullptr

};

#endif // MEDIASTACK_CLASS_H