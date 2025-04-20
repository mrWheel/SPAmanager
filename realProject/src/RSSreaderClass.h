#ifndef RSSREADERCLASS_H
#define RSSREADERCLASS_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <vector>
#include <ctime>

class RSSreaderClass {
public:
  RSSreaderClass();

  void loop(struct tm timeNow); 
  void setDebug(Stream* debugPort) { debug = debugPort; }
  void setRequestInterval(uint32_t interval) { _interval = interval * 60 * 1000; }
  bool addRSSfeed(const char* url, const char* path, size_t maxFeeds);
  bool getNextFeedItem(uint8_t& feedIndex, size_t& itemIndex);
  String readRssFeed(uint8_t feedIndex, size_t itemIndex);

private:
  WiFiClientSecure secureClient;
  String _urls[10];
  String _paths[10];
  String _filePaths[10];
  uint8_t _activeFeedCount = 0;
  size_t _maxFeedsPerFile[10] = {0};
  unsigned long _interval = 12000000; // standaard 20 min
  unsigned long _lastCheck = 0;

  uint8_t _feedCheckState = 0;         // Current feed being checked
  unsigned long _lastFeedCheck = 0;     // Time of last individual feed check
  unsigned long _feedCheckInterval = 60000; // Time between checking individual feeds (60 seconds)
  bool _checkingFeeds = false;          // Flag to indicate we're in the middle of checking feeds
  uint8_t _currentFeedIndex = 0;
  uint8_t _currentItemIndex = 0;
  size_t _currentItemIndices[10] = {0}; // Track current item index for each feed
  uint8_t _feedReadCounts[10] = {0}; // Track how many items we've read from each feed
  uint16_t _totalMaxFeeds = 0;       // Sum of all maxFeeds values
  
  String fetchFeed(const char* host, const char* path);
  std::vector<String> extractTitles(const String& feed);
  std::vector<String> getStoredLines(uint8_t feedIndex);
  void saveTitles(const std::vector<String>& titles, uint8_t feedIndex);
  bool titleExists(const String& title, const std::vector<String>& lines);
  void checkForNewFeedItems();
  void checkFeed(uint8_t feedIndex);
  void deleteFeedsOlderThan(struct tm timeNow);
  Stream* debug = nullptr; // Optional, default to nullptr

  #ifdef RSSREADER_DEBUG
    bool doDebug = true;
  #else
    bool doDebug = false;
  #endif

};

#endif