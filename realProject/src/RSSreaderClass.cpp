#include "RSSreaderClass.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <LittleFS.h>

RSSreaderClass::RSSreaderClass() 
{
  _activeFeedCount = 0;

} // RSSreaderClass()

bool RSSreaderClass::addRSSfeed(const char* url, const char* path, size_t maxFeeds) 
{
  if (_activeFeedCount >= 10)
  {
    if (debug) debug->println("RSSreaderClass::addRSSfeed(): Maximum number of feeds reached");
    return false;
  }
  
  uint8_t feedIndex = _activeFeedCount;
  _urls[feedIndex] = url;
  _paths[feedIndex] = path;
  _filePaths[feedIndex] = "/RSSfeed" + String(feedIndex) + ".txt";
  _maxFeedsPerFile[feedIndex] = maxFeeds;
  _totalMaxFeeds += maxFeeds; // Update total max feeds
  _lastCheck = 0;

  if (debug && doDebug) debug->printf("RSSreaderClass::addRSSfeed(): URL:[%s], Path:[%s], File:[%s], Max Feeds:[%d], Index: [%d]\n" 
                                                                  , _urls[feedIndex].c_str()
                                                                  , _paths[feedIndex].c_str()
                                                                  , _filePaths[feedIndex].c_str()
                                                                  , _maxFeedsPerFile[feedIndex], feedIndex);

  LittleFS.begin();
  if (!LittleFS.exists(_filePaths[feedIndex])) 
  {
    if (debug) debug->printf("RSSreaderClass::addRSSfeed(): Bestand [%s] bestaat niet, aanmaken...\n", _filePaths[feedIndex].c_str());
    File file = LittleFS.open(_filePaths[feedIndex], "w");
    if (file) 
    {
      file.close();
      if (debug && doDebug) debug->printf("RSSreaderClass::addRSSfeed(): Bestand [%s]aangemaakt.\n", _filePaths[feedIndex].c_str());
    } 
    else 
    {
      if (debug) debug->printf("RSSreaderClass::addRSSfeed(): Kan bestand [%s] niet aanmaken.\n", _filePaths[feedIndex].c_str()); 
      return false;
    }
  } 
  else 
  {
    if (debug && doDebug) debug->println("RSSreaderClass::addRSSfeed(): Bestand bestaat al.");
  }
  
  _activeFeedCount++;  // Increment FIRST
  checkFeed(feedIndex);  // Then check the feed we just added
  return true;

} // addRSSfeed()


void RSSreaderClass::loop(struct tm timeNow) 
{
  // Main interval check - start the feed checking process
  if (!_checkingFeeds && (millis() - _lastCheck >= _interval)) 
  {
    // Start the feed checking process
    deleteFeedsOlderThan(timeNow);
    _checkingFeeds = true;
    _feedCheckState = 0;
    _lastFeedCheck = 0; // Force immediate check of first feed
    
    if (debug && doDebug) debug->println("RSSreaderClass::loop(): Starting feed check cycle");
  }
  
  // If we're in the process of checking feeds, check one feed at a time
  if (_checkingFeeds && (millis() - _lastFeedCheck >= _feedCheckInterval)) 
  {
    if (_feedCheckState < _activeFeedCount) 
    {
      // Check the current feed
      if (debug && doDebug) debug->printf("RSSreaderClass::loop(): Checking feed[%d] of [%d]\n", 
                              _feedCheckState + 1, _activeFeedCount);
      
      checkFeed(_feedCheckState);
      _lastFeedCheck = millis();
      _feedCheckState++;
    } 
    else 
    {
      // All feeds checked, end the cycle
      if (debug && doDebug) debug->println("RSSreaderClass::loop(): Feed check cycle complete");
      _checkingFeeds = false;
      _lastCheck = millis(); // Reset the main interval timer
    }
  }

} // loop()


String RSSreaderClass::fetchFeed(const char* host, const char* path) 
{
  if (debug) debug->printf("RSSreaderClass::fetchFeed(): URL[%s], PATH[%s]\n", host, path);

//if (debug && doDebug) debug->printf("RSSreaderClass::fetchFeed(): host[%s], path[%s]\n", host.c_str(), path.c_str());

  WiFiClientSecure client;
  client.setInsecure();  // Voor testdoeleinden
  client.setTimeout(5000); // 5s timeout

  if (!client.connect(host, 443)) 
  {
    if (debug && doDebug) debug->println("RSSreaderClass::fetchFeed(): Verbinding mislukt!");
    return "Fout: Kon geen TLS verbinding maken";
  }

  client.println("GET /" + String(path) + " HTTP/1.1");
  client.println("Host: " + String(host));
  client.println("User-Agent: ESP32RSSReader/1.0");
  client.println("Connection: close");
  client.println();

  if (debug && doDebug) debug->println("RSSreaderClass::fetchFeed(): Verzoek verzonden, wacht op antwoord...");

  // Antwoord inlezen (met totale timeout)
  String response;
  unsigned long startTime = millis();
  const unsigned long maxDuration = 8000;

  while ((client.connected() || client.available()) && millis() - startTime < maxDuration) 
  {
    if (client.available()) 
    {
      String line = client.readStringUntil('\n');
      response += line + "\n";
    } else {
      delay(10);
    }
  }

  client.stop();
  if (debug && doDebug) debug->println("RSSreaderClass::fetchFeed(): Verbinding gesloten");

  // Strip headers
  int bodyIndex = response.indexOf("\r\n\r\n");
  if (bodyIndex == -1) 
  {
    if (debug && doDebug) debug->println("RSSreaderClass::fetchFeed(): Kon body niet vinden in antwoord");
    return "Fout: HTTP headers niet correct";
  }

  String xmlPayload = response.substring(bodyIndex + 4);
  if (debug && xmlPayload.length() == 0) 
  {
    debug->printf("RSSreaderClass::fetchFeed(): Payload length: %d\n", xmlPayload.length());
    //debug->println(xmlPayload.substring(0, 300));
  }
  if (debug && doDebug) 
  {
    debug->printf("RSSreaderClass::fetchFeed(): Payload length: %d\n", xmlPayload.length());
    //debug->println(xmlPayload.substring(0, 300));
  }

  return xmlPayload;

} // fetchFeed()


std::vector<String> RSSreaderClass::extractTitles(const String& feed) 
{
  std::vector<String> titles;
  int pos = 0;
  while ((pos = feed.indexOf("<title>", pos)) != -1) 
  {
    int start = pos + 7;
    int end = feed.indexOf("</title>", start);
    if (end == -1) break;
    String title = feed.substring(start, end);
    title.trim();
    
    // Add code to remove CDATA tags
    if (title.startsWith("<![CDATA[") && title.endsWith("]]>")) 
    {
      title = title.substring(9, title.length() - 3);
    }
    
    pos = end + 8;
    if (title.length() > 0) titles.push_back(title);
  }

  if (!titles.empty()) titles.erase(titles.begin()); // verwijder kanaaltitel
  return titles;

} // extractTitles()

std::vector<String> RSSreaderClass::getStoredLines(uint8_t feedIndex) 
{
  std::vector<String> lines;
  if (feedIndex >= _activeFeedCount) return lines;
  
  LittleFS.begin();
  File file = LittleFS.open(_filePaths[feedIndex], "r");
  if (!file) return lines;

  while (file.available()) 
  {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) lines.push_back(line);
  }

  file.close();
  return lines;

} // getStoredLines()

void RSSreaderClass::saveTitles(const std::vector<String>& titles, uint8_t feedIndex) 
{
  if (feedIndex >= _activeFeedCount) return;
  
  LittleFS.begin();
  File file = LittleFS.open(_filePaths[feedIndex], "a");
  if (!file) 
  {
    if (debug && doDebug) debug->println("RSSreaderClass::saveTitles(): Kan bestand niet openen voor schrijven");
    return;
  }

  for (const String& title : titles) 
  {
    time_t now = time(nullptr);
    if (debug && doDebug) debug->printf("RSSreaderClass::saveTitles(): Titel: %s\n", title.c_str());
    file.print(now);
    file.print("|");
    file.println(title);
  }

  file.close();

} // saveTitles()

bool RSSreaderClass::titleExists(const String& title, const std::vector<String>& lines) 
{
  for (const auto& line : lines) 
  {
    int sep = line.indexOf('|');
    if (sep != -1) 
    {
      String existingTitle = line.substring(sep + 1);
      if (existingTitle == title) return true;
    }
  }
  return false;

} // titleExists()

void RSSreaderClass::checkFeed(uint8_t feedIndex)
{
  if (feedIndex >= _activeFeedCount)
  {
    if (debug && doDebug) debug->printf("RSSreaderClass::checkFeed(): Invalid feed index: %d\n", feedIndex);
    return;
  }
  
  if (debug && doDebug) debug->printf("RSSreaderClass::checkFeed(): Checking feed %d: %s\n", 
                          feedIndex, _urls[feedIndex].c_str());
                          
  String feed = fetchFeed(_urls[feedIndex].c_str(), _paths[feedIndex].c_str());

  if (feed.startsWith("Fout:")) 
  {
    if (debug) debug->printf("RSSreaderClass::checkFeed(): Fout bij ophalen RSS-feed: [%s]\n", feed);
    return;
  }

  std::vector<String> existing = getStoredLines(feedIndex);
  std::vector<String> newTitles = extractTitles(feed);
  std::vector<String> updated = existing;
  bool changed = false;

  for (const auto& title : newTitles) 
  {
    if (!titleExists(title, existing)) 
    {
      if (debug && doDebug) debug->printf("RSSreaderClass::checkFeed(): Nieuwe titel gevonden: %s\n", title.c_str());
      time_t now = time(nullptr);
      updated.push_back(String(now) + "|" + title);
      changed = true;
    }
  }

  // FIFO: maximaal _maxFeedsPerFile titels
  while (updated.size() > _maxFeedsPerFile[feedIndex]) 
  {
    updated.erase(updated.begin());
  }

  if (changed) 
  {
    LittleFS.begin();
    File file = LittleFS.open(_filePaths[feedIndex], "w"); 
    if (file) 
    {
      for (const auto& line : updated) 
      {
        file.println(line);
      }
      file.close();
      if (debug && doDebug) debug->println("RSSreaderClass::checkFeed(): Nieuwe feeds toegevoegd.");
      if (debug) debug->printf("RSSreaderClass::checkFeed(): Feed[%d] now has [%d] items\n", feedIndex, updated.size());
    } 
    else 
    {
      if (debug) debug->println("RSSreaderClass::checkFeed(): Kan niet naar bestand schrijven.");
    }
  } 
  else 
  {
    if (debug && doDebug) debug->println("RSSreaderClass::checkFeed(): Geen nieuwe titels.");
  }
} // checkFeed()


void RSSreaderClass::checkForNewFeedItems() 
{
  if (debug && doDebug) debug->println("RSSreaderClass::checkForNewFeedItems(): Checken op nieuwe RSS-items...");
  
  for (uint8_t feedIndex = 0; feedIndex < _activeFeedCount; feedIndex++)
  {
    if (debug && doDebug) debug->printf("RSSreaderClass::checkForNewFeedItems(): Checking feed %d: %s\n", 
                            feedIndex, _urls[feedIndex].c_str());
                            
    String feed = fetchFeed(_urls[feedIndex].c_str(), _paths[feedIndex].c_str());

    if (feed.startsWith("Fout:")) 
    {
      if (debug) debug->printf("RSSreaderClass::checkForNewFeedItems(): Fout bij ophalen RSS-feed: [%s]\n", feed);
      continue;
    }

    std::vector<String> existing = getStoredLines(feedIndex);
    std::vector<String> newTitles = extractTitles(feed);
    std::vector<String> updated = existing;
    bool changed = false;

    for (const auto& title : newTitles) 
    {
      if (!titleExists(title, existing)) 
      {
        if (debug && doDebug) debug->printf("RSSreaderClass::checkForNewFeedItems(): Nieuwe titel gevonden: %s\n", title.c_str());
        time_t now = time(nullptr);
        updated.push_back(String(now) + "|" + title);
        changed = true;
      }
    }

    // FIFO: maximaal _maxFeedsPerFile titels
    while (updated.size() > _maxFeedsPerFile[feedIndex]) 
    {
      updated.erase(updated.begin());
    }

    if (changed) 
    {
      LittleFS.begin();
      File file = LittleFS.open(_filePaths[feedIndex], "w"); 
      if (file) 
      {
        for (const auto& line : updated) 
        {
          file.println(line);
        }
        file.close();
        if (debug && doDebug) debug->println("RSSreaderClass::checkForNewFeedItems(): Nieuwe feeds toegevoegd.");
        if (debug) debug->printf("RSSreaderClass::checkForNewFeedItems(): Feed[%d] now has [%d] items\n", feedIndex, updated.size());
      } 
      else 
      {
        if (debug) debug->println("RSSreaderClass::checkForNewFeedItems(): Kan niet naar bestand schrijven.");
      }
    } 
    else 
    {
      if (debug && doDebug) debug->println("RSSreaderClass::checkForNewFeedItems(): Geen nieuwe titels.");
    }
  }

} // checkForNewFeedItems()

void RSSreaderClass::deleteFeedsOlderThan(struct tm timeNow) 
{
  if (debug && doDebug) debug->println("RSSreaderClass::deleteFeedsOlderThan(): Verwijderen van oude feeds...");

  // Calculate timestamp for 24 hours ago
  time_t now = time(nullptr);
  time_t twentyFourHoursAgo = now - (48 * 60 * 60); // 48 hours in seconds

  if (debug && doDebug) debug->printf("RSSreaderClass::deleteFeedsOlderThan(): Current time: %ld, 48 hours ago: %ld\n", 
                          now, twentyFourHoursAgo);

  for (uint8_t feedIndex = 0; feedIndex < _activeFeedCount; feedIndex++)
  {
    std::vector<String> lines = getStoredLines(feedIndex);
    std::vector<String> filtered;

    for (const auto& line : lines) 
    {
      int sep = line.indexOf('|');
      if (sep != -1) 
      {
        time_t ts = atol(line.substring(0, sep).c_str());
        if (ts >= twentyFourHoursAgo) 
        {
          filtered.push_back(line);
        }
        else if (debug && doDebug)
        {
          debug->printf("RSSreaderClass::deleteFeedsOlderThan(): Removing old item: %s (timestamp: %ld)\n", 
                        line.c_str(), ts);
        }
      }
    }

    LittleFS.begin();
    File file = LittleFS.open(_filePaths[feedIndex], "w");
    if (file) 
    {
      for (const auto& line : filtered) 
      {
        file.println(line);
      }
      file.close();
    }

    if (debug && doDebug) debug->printf("RSSreaderClass::deleteFeedsOlderThan(): Feed[%d], Feeds behouden[%d] / verwijderd[%d]\n", 
                            feedIndex, filtered.size(), lines.size() - filtered.size());
  }
} // deleteFeedsOlderThan()


bool RSSreaderClass::getNextFeedItem(uint8_t& feedIndex, size_t& itemIndex)
{
  if (_activeFeedCount == 0)
  {
    if (debug) debug->println("RSSreaderClass::getNextFeedItem(): No active feeds");
    return false;
  }
  
  // Return current feed and item indices
  feedIndex = _currentFeedIndex;
  itemIndex = _currentItemIndices[_currentFeedIndex];
  
  // Increment the item index for this feed
  _currentItemIndices[_currentFeedIndex]++;
  
  // Check if we've reached the end of the current feed
  std::vector<String> lines = getStoredLines(_currentFeedIndex);
  if (_currentItemIndices[_currentFeedIndex] >= lines.size())
  {
    // Reset to beginning of this feed
    _currentItemIndices[_currentFeedIndex] = 0;
  }
  
  // Increment read counter for current feed
  _feedReadCounts[_currentFeedIndex]++;
  
  // Determine which feed to read from next using deficit round-robin
  if (_activeFeedCount > 1)
  {
    // Calculate ideal read ratios for each feed
    float totalItems = 0;
    for (uint8_t i = 0; i < _activeFeedCount; i++)
    {
      totalItems += _maxFeedsPerFile[i];
    }
    
    // Find feed with largest deficit between ideal and actual reads
    float maxDeficit = -1.0;
    uint8_t nextFeed = (_currentFeedIndex + 1) % _activeFeedCount; // Default to next feed
    
    uint32_t totalReads = 0;
    for (uint8_t i = 0; i < _activeFeedCount; i++)
    {
      totalReads += _feedReadCounts[i];
    }
    
    if (totalReads > 0) // Avoid division by zero
    {
      for (uint8_t i = 0; i < _activeFeedCount; i++)
      {
        float idealRatio = _maxFeedsPerFile[i] / totalItems;
        float actualRatio = (float)_feedReadCounts[i] / (float)totalReads;
        float deficit = idealRatio - actualRatio;
        
        if (deficit > maxDeficit)
        {
          maxDeficit = deficit;
          nextFeed = i;
        }
      }
    }
    
    _currentFeedIndex = nextFeed;
  }
  else
  {
    // Only one feed, stay on it
    _currentFeedIndex = 0;
  }
  
  // Reset counters if we've read a lot of items to prevent overflow
  if (_feedReadCounts[0] > 1000) // Arbitrary large number
  {
    for (uint8_t i = 0; i < _activeFeedCount; i++)
    {
      _feedReadCounts[i] = _feedReadCounts[i] / 2; // Reduce all proportionally
    }
  }
  
  return true;

} // getNextFeedItem()


String RSSreaderClass::readRssFeed(uint8_t feedIndex, size_t itemIndex) 
{
  if (feedIndex >= _activeFeedCount)
  {
    if (debug) debug->printf("RSSreaderClass::readRssFeed(): Invalid feed index[%d]\n", feedIndex);
    return "";
  }
  
  if (debug && doDebug) debug->printf("RSSreaderClass::readRssFeed(): FeedIndex[%d], ItemIndex[%d]\n", feedIndex, itemIndex);
  std::vector<String> lines = getStoredLines(feedIndex);
  
  if (itemIndex >= lines.size()) 
  {
    if (debug) debug->printf("RSSreaderClass::readRssFeed(): Geen RSS-feed gevonden voor nummer [%d] in feed[%d]\n", 
                            itemIndex, feedIndex);
    return "";
  }
  
  int sep = lines[itemIndex].indexOf('|');
  if (sep != -1) 
  {
    if (debug && doDebug) debug->printf("RSSreaderClass::readRssFeed(): Feed[%d], Item[%d] - [%s]\n", 
                            feedIndex, itemIndex, lines[itemIndex].c_str());
    return lines[itemIndex].substring(sep + 1);
  }

  return "";

} // RSSreaderClass::readRssFeed()