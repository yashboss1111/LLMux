#pragma once

// Debug log
#define DEBUG_INFORMATION_FORMAT \
    "File '%s': line %u in function '%s' | Message: "
#define DEBUG_INFORMATION_TO_PRINT __FILE__, __LINE__, __func__

#define logError( _format, ... )                       \
    fprintf( stderr, DEBUG_INFORMATION_FORMAT _format, \
             DEBUG_INFORMATION_TO_PRINT, ##__VA_ARGS__ )

#define log logError
