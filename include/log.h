#pragma once

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Configuration
#define LOG_FILE_PATH "log.txt"

#if defined( DEBUG )

// Debug log
#define DEBUG_INFORMATION_FORMAT \
    "File '%s': line %u in function '%s' | Message: "
#define DEBUG_INFORMATION_TO_PRINT __FILE__, __LINE__, __func__

#define logError( _format, ... ) do {                       \
    const char* l_userFormat = (_format);                                          \
    size_t l_fullFormatLength = ( strlen(DEBUG_INFORMATION_FORMAT) + strlen(l_userFormat) + 1 );                \
    char* l_fullFormat = (char*)malloc( l_fullFormatLength );                                   \
    strcpy( l_fullFormat, DEBUG_INFORMATION_FORMAT );                                      \
    strcat( l_fullFormat, l_userFormat );                                       \
    fprintf( stderr, l_fullFormat, \
            DEBUG_INFORMATION_TO_PRINT, ##__VA_ARGS__ ); \
    free( l_fullFormat );\
} while( 0 )

#define log logError

#else

#define log

#endif
