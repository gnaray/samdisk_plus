#ifndef OPTIONS_H
#define OPTIONS_H

#include "Header.h"
#include "Range.h"

#include <string>

#define MAX_PATH    512

enum class PreferredData { Unknown, Track, Bitstream, Flux };

enum { GAPS_AUTO = -1, GAPS_NONE, GAPS_CLEAN, GAPS_ALL };

typedef char charArrayMAX_PATH[MAX_PATH];

template<typename T>
T& getOpt(const std::string& key);

template<>
int& getOpt(const std::string& key);

template<>
bool& getOpt(const std::string& key);

template<>
long& getOpt(const std::string& key);

template<>
Range& getOpt(const std::string& key);

template<>
Encoding& getOpt(const std::string& key);

template<>
DataRate& getOpt(const std::string& key);

template<>
PreferredData& getOpt(const std::string& key);

template<>
std::string& getOpt(const std::string& key);

template<>
charArrayMAX_PATH& getOpt(const std::string& key);

#endif // OPTIONS_H
