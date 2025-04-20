#include "LocalMessagesClass.h"

LocalMessagesClass::LocalMessagesClass(const std::string& path, size_t recordSize)
    : path(path), recordSize(recordSize) {}

void LocalMessagesClass::setDebug(Stream* debugPort) {
    debug = debugPort;
}

std::string LocalMessagesClass::read(uint8_t recNr) {
    File file = LittleFS.open(path.c_str(), "r");
    if (!file) {
        if (debug) debug->printf("read(): Failed to open file: %s\n", path.c_str());
        return "";
    }

    size_t offset = recNr * recordSize;
    if (!file.seek(offset)) {
        if (debug) debug->printf("read(): Failed to seek to record %d (offset %d)\n", recNr, offset);
        file.close();
        return "";
    }

    char buffer[recordSize + 1];
    size_t bytesRead = file.readBytes(buffer, recordSize);
    buffer[bytesRead] = '\0';

    file.close();

    if (bytesRead == 0) {
        if (debug) debug->printf("read(): No data read for record %d\n", recNr);
        return "";
    }

    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }

    return std::string(buffer);
}

bool LocalMessagesClass::write(uint8_t recNr, const char* data) {
    if (recNr == 0) {
        if (debug) debug->println("write(): record number is zero, erase file");
        LittleFS.remove(path.c_str());
    }

    const char* start = data;
    while (*start == ' ') start++;

    const char* end = data + strlen(data);
    while (end > start && *(end - 1) == ' ') end--;

    size_t trimmedLen = end - start;

    if (trimmedLen == 0) {
        if (debug) debug->println("Skipped writing empty or space-only localMessage after trimming.");
        return false;
    }

    File file = LittleFS.open(path.c_str(), "a");
    if (!file) {
        if (debug) debug->printf("write(): Failed to open file: %s\n", path.c_str());
        return false;
    }

    size_t offset = recNr * recordSize;
    if (!file.seek(offset)) {
        if (debug) debug->printf("write(): Failed to seek to record %d (offset %d)\n", recNr, offset);
        file.close();
        return false;
    }

    char buffer[recordSize];
    memset(buffer, 0, recordSize);

    if (trimmedLen > recordSize - 1) {
        trimmedLen = recordSize - 1;
    }

    memcpy(buffer, start, trimmedLen);
    buffer[trimmedLen] = '\n';

    size_t bytesWritten = file.write((uint8_t*)buffer, recordSize);
    file.flush();
    file.close();

    if (bytesWritten != recordSize) {
        if (debug) debug->printf("write(): Incomplete write: expected %d bytes, wrote %d bytes\n", recordSize, bytesWritten);
        return false;
    }

    if (debug) debug->printf("Successfully wrote record %d: '%.*s'\n", recNr, (int)trimmedLen, start);
    return true;
}