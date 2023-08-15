#ifndef OPTIONS_H
#define OPTIONS_H

#include "Header.h"
#include "Range.h"
#include "FileIO.h" // MAX_PATH

#include <string>

enum class PreferredData { Unknown, Track, Bitstream, Flux };

enum { GAPS_AUTO = -1, GAPS_NONE, GAPS_CLEAN, GAPS_ALL };

typedef char charArrayMAX_PATH[MAX_PATH];

constexpr const char* DETECT_FS_AUTO = "auto";

template<typename T>
T& getOpt(const char* key);

template<typename T>
inline T& getOpt(const std::string& key)
{
    return getOpt<T>(key.c_str());
}

template<>
int& getOpt(const char* key);

template<>
bool& getOpt(const char* key);

template<>
long& getOpt(const char* key);

template<>
Range& getOpt(const char* key);

template<>
Encoding& getOpt(const char* key);

template<>
DataRate& getOpt(const char* key);

template<>
PreferredData& getOpt(const char* key);

template<>
std::string& getOpt(const char* key);

template<>
charArrayMAX_PATH& getOpt(const char* key);

#endif // OPTIONS_H
