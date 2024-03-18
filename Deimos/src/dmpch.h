#ifndef ENGINE_DMPCH_H
#define ENGINE_DMPCH_H

#include <iostream>
#include <utility>
#include <algorithm>
#include <functional>
#include <memory>

#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "Deimos/Log.h"

#ifdef DM_PLATFORM_WINDOWS
    #include <Windows.h>
#elif DM_PLATFORM_LINUX
    // #include <unistd.h>
#endif

#endif //ENGINE_DMPCH_H
