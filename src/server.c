#include <civetweb.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"

// Configuration
#define MAX_RESPONSE_SIZE 4096
#define LLM_CHAT_EXECUTABLE_NAME "llm_chat"
#define SERVE_DIRECTORY "www"
#define SERVER_PORT "8080"

static int g_pipeToChild[ 2 ];
static int g_pipeFromChild[ 2 ];

static void truncateString( char** _string,
                            const ssize_t _from,
                            const ssize_t _to ) {
    if ( _from >= 0 ) {
        ( *_string ) += _from;
    }

    if ( _to >= 0 ) {
        ( *_string )[ _to ] = '\0';
    }
}

static size_t concatBeforeAndAfterString( char** _string,
                                          const char* _beforeString,
                                          const char* _afterString ) {
    size_t l_returnValue = 0;

    {
        if ( !_string || !*_string ) {
            goto EXIT;
        }

        const size_t l_stringLength = strlen( *_string );

        const size_t l_beforeStringLength =
            ( ( _beforeString ) ? ( strlen( _beforeString ) ) : ( 0 ) );

        const size_t l_afterStringLegnth =
            ( ( _afterString ) ? ( strlen( _afterString ) ) : ( 0 ) );

        const size_t l_totalLength =
            ( l_beforeStringLength + l_stringLength + l_afterStringLegnth );

        if ( !l_totalLength ) {
            goto EXIT;
        }

        {
            // String
            {
                *_string = ( char* )realloc(
                    *_string, ( l_totalLength + 1 ) * sizeof( char ) );

                if ( l_stringLength && l_beforeStringLength ) {
                    memcpy( ( l_beforeStringLength + *_string ), *_string,
                            l_stringLength );
                }
            }

            // Before
            if ( l_beforeStringLength ) {
                memcpy( *_string, _beforeString, l_beforeStringLength );
            }

            // After
            if ( l_afterStringLegnth ) {
                memcpy( ( l_beforeStringLength + l_stringLength + *_string ),
                        _afterString, l_afterStringLegnth );
            }

            ( *_string )[ l_totalLength ] = '\0';
        }

        l_returnValue = l_totalLength;
    }

EXIT:
    return ( l_returnValue );
}

static char* getApplicationDirectoryAbsolutePath( void ) {
    char* l_returnValue = NULL;

    {
        char* l_executablePath = ( char* )malloc( PATH_MAX * sizeof( char ) );

        // Get executable path
        {
            ssize_t l_executablePathLength = readlink(
                "/proc/self/exe", l_executablePath, ( PATH_MAX - 1 ) );

            if ( l_executablePathLength == -1 ) {
                log( "readlink\n" );

                free( l_executablePath );

                goto EXIT;
            }

            l_executablePath[ l_executablePathLength ] = '\0';
        }

        char* l_directoryPath = NULL;

        // Get directory path
        {
            char* l_lastSlash = strrchr( l_executablePath, '/' );

            if ( !l_lastSlash ) {
                log( "Extracting directory: '%s'\n", l_executablePath );

                goto EXIT;
            }

            const ssize_t l_lastSlashIndex = ( l_lastSlash - l_executablePath );

            l_directoryPath = l_executablePath;

            // Do not move the beginning
            truncateString( &l_directoryPath, -1, l_lastSlashIndex );

            if ( !concatBeforeAndAfterString( &l_directoryPath, NULL, "/" ) ) {
                free( l_directoryPath );

                goto EXIT;
            }
        }

        l_returnValue = l_directoryPath;
    }

EXIT:
    return ( l_returnValue );
}

// TODO: Dynamic response size
#if 0
static char* readFileDescriptorToString( int _fileDescriptor ) {
    size_t l_capacity = 1024;
    size_t l_length = 0;
    char* l_buffer = ( char* )malloc( l_capacity * sizeof( char ) );

    log( "TEST1\n" );

    while ( true ) {
        ssize_t l_readAmount = read( _fileDescriptor, ( l_buffer + l_length ),
                                     ( l_capacity - l_length - 1 ) );

        if ( l_readAmount > 0 ) {
            l_length += l_readAmount;

            if ( ( l_length + 1 ) >= l_capacity ) {
                l_capacity *= 2;

                char* l_tempBuffer = ( char* )realloc( l_buffer, l_capacity );

                if ( !l_tempBuffer ) {
                    free( l_buffer );

                    log( "TEST3\n" );
                    return ( NULL );
                }

                l_buffer = l_tempBuffer;
            }

        } else if ( l_readAmount == 0 ) {
            // EOF
            log( "TEST4\n" );
            break;

        } else if ( errno == EINTR ) {
            // Retry
            log( "TEST5\n" );
            continue;

        } else {
            free( l_buffer );

            log( "TEST6\n" );
            return ( NULL );
        }
    }

    l_buffer[ l_length ] = '\0';

    log( "TEST2\n" );

    return ( l_buffer );
}
#endif

static bool openLLMChatExecutable( void ) {
    bool l_returnValue = false;

    {
        // Directory path
        char* l_executablePath = getApplicationDirectoryAbsolutePath();

        if ( !l_executablePath ) {
            log( "Failed to get executable directory\n" );

            goto EXIT;
        }

        if ( !concatBeforeAndAfterString( &l_executablePath, NULL,
                                          "/" LLM_CHAT_EXECUTABLE_NAME ) ) {
            free( l_executablePath );

            log( "Failed to get shared object directory\n" );

            goto EXIT;
        }

        pipe( g_pipeToChild );
        pipe( g_pipeFromChild );

        pid_t l_processId = fork();

        if ( l_processId == 0 ) {
            // CHILD PROCESS
            dup2( g_pipeToChild[ 0 ], STDIN_FILENO ); // stdin - pipe read end
            dup2( g_pipeFromChild[ 1 ],
                  STDOUT_FILENO ); // stdout - pipe write end

            int l_logFileDescriptor =
                open( "log.txt",
                      O_WRONLY       // Open for writing only
                          | O_CREAT  // Create the file if it doesn’t exist
                          | O_TRUNC, // Truncate (zero out) the file on open
                      S_IRUSR        // owner has read permission (0400)
                          | S_IWUSR  // owner has write permission (0200)
                          | S_IRGRP  // group has read permission (0040)
                          | S_IROTH  // others have read permission (0004)
                );

            if ( l_logFileDescriptor == -1 ) {
                _exit( 1 );
            }

            dup2( l_logFileDescriptor, STDERR_FILENO );

            close( l_logFileDescriptor );

            // Close unused ends
            close( g_pipeToChild[ 1 ] );
            close( g_pipeFromChild[ 0 ] );

            execlp( l_executablePath, l_executablePath, NULL );

            log( "execlp failed\n" );

            l_returnValue = false;

        } else {
            // PARENT PROCESS
            // Close unused ends
            close( g_pipeToChild[ 0 ] );
            close( g_pipeFromChild[ 1 ] );
        }

        free( l_executablePath );

        l_returnValue = true;
    }

EXIT:
    return ( l_returnValue );
}

static char* llm_generate( const char* _prompt ) {
    log( "Prompt: %s\n", _prompt );
    printf( "Prompt: %s\n", _prompt );

    {
        // Append new line
        char* l_prompt = strdup( _prompt );
        concatBeforeAndAfterString( &l_prompt, NULL, "\n" );

        // Write to child
        write( g_pipeToChild[ 1 ], l_prompt, strlen( l_prompt ) );

        free( l_prompt );
    }

    // TODO: Dynamic response size
#if 0
    char* l_response = readFileDescriptorToString( g_pipeFromChild[ 0 ] );

    if ( !l_response ) {
        log( "Failed to read from LLM chat\n" );

        exit( 1 );
    }
#endif

    char l_response[ MAX_RESPONSE_SIZE ];
    ssize_t l_readAmount =
        read( g_pipeFromChild[ 0 ], l_response, ( sizeof( l_response ) - 1 ) );

    if ( l_readAmount == -1 ) {
        exit( 1 );
    }

    l_response[ l_readAmount ] = '\0';

    log( "LLM Response: %s", l_response );
    printf( "LLM Response: %s", l_response );

    return ( strdup( l_response ) );
}

static bool initServer( struct mg_context** _serverContext,
                        const char** _options ) {
    if ( !_serverContext ) {
        return ( false );
    }

    if ( !_options ) {
        return ( false );
    }

    struct mg_callbacks l_callbacks;
    memset( &l_callbacks, 0, sizeof( l_callbacks ) );

    *_serverContext = mg_start( &l_callbacks, NULL, _options );

    if ( *_serverContext == NULL ) {
        log( "Failed to start civetweb\n" );

        return ( false );
    }

    return ( true );
}

static bool quitServer( struct mg_context* _serverContext ) {
    if ( !_serverContext ) {
        return ( false );
    }

    mg_stop( _serverContext );

    return ( true );
}

static char* readRequestBody( struct mg_connection* _connection ) {
    size_t l_capacity = 1024;
    size_t l_length = 0;
    char* l_buffer = ( char* )malloc( l_capacity * sizeof( char ) );

    while ( true ) {
        int l_readAmount = mg_read( _connection, ( l_buffer + l_length ),
                                    ( l_capacity - l_length - 1 ) );

        if ( l_readAmount ) {
            l_length += l_readAmount;

            if ( ( l_length + 1 ) >= l_capacity ) {
                l_capacity *= 2;

                char* l_tempBuffer = realloc( l_buffer, l_capacity );

                if ( !l_tempBuffer ) {
                    free( l_buffer );

                    return ( NULL );
                }

                l_buffer = l_tempBuffer;
            }

        } else {
            // EOF ( l_readAmount == 0 ) or Error ( l_readAmount < 0 )
            break;
        }
    }

    l_buffer[ l_length ] = '\0';

    return ( l_buffer );
}

static char* extractPrompt( const char* _requestBody ) {
    const char* l_promptStartIndex = strstr( _requestBody, "\"prompt\"" );

    if ( !l_promptStartIndex ) {
        return ( NULL );
    }

    l_promptStartIndex = strchr( l_promptStartIndex, ':' );

    if ( !l_promptStartIndex ) {
        return ( NULL );
    }

    // Skip ':'
    l_promptStartIndex++;

    while ( *l_promptStartIndex &&
            isspace( ( unsigned char )*l_promptStartIndex ) ) {
        l_promptStartIndex++;
    }

    if ( *l_promptStartIndex != '"' ) {
        return ( NULL );
    }

    // Skip opening quote
    l_promptStartIndex++;

    // Find closing unescaped quote
    const char* l_promptEndIndex = l_promptStartIndex;

    while ( *l_promptEndIndex ) {
        if ( ( *l_promptEndIndex == '"' ) &&
             ( l_promptEndIndex[ -1 ] != '\\' ) ) {
            break;
        }

        l_promptEndIndex++;
    }

    if ( *l_promptEndIndex != '"' ) {
        return ( NULL );
    }

    size_t l_promptLength = ( l_promptEndIndex - l_promptStartIndex );
    char* l_prompt = ( char* )malloc( ( l_promptLength + 1 ) * sizeof( char ) );

    if ( !l_prompt ) {
        return ( NULL );
    }

    strncpy( l_prompt, l_promptStartIndex, l_promptLength );

    l_prompt[ l_promptLength ] = '\0';

    // TODO: Unescape sequences in prompt

    return ( l_prompt );
}

static int handleGenerate( struct mg_connection* _connection, void* _cbdata ) {
    ( void )( sizeof( _cbdata ) );

    char* l_requestBody = readRequestBody( _connection );

    if ( !l_requestBody ) {
        mg_printf( _connection,
                   "HTTP/1.1 500 Internal Server Error\r\n"
                   "Content-Length: 0\r\n\r\n" );

        return ( 500 );
    }

    char* l_prompt = extractPrompt( l_requestBody );

    free( l_requestBody );

    if ( !l_prompt ) {
        mg_printf( _connection,
                   "HTTP/1.1 400 Bad Request\r\n"
                   "Content-Type: text/plain\r\n"
                   "Content-Length: 11\r\n\r\n"
                   "Bad Request" );

        return ( 400 );
    }

    char* l_response = llm_generate( l_prompt );

    free( l_prompt );

    if ( !l_response ) {
        mg_printf( _connection,
                   "HTTP/1.1 502 Bad Gateway\r\n"
                   "Content-Type: text/plain\r\n"
                   "Content-Length: 11\r\n\r\n"
                   "Server Error" );

        return ( 502 );
    }

    char* l_JSONResponse = NULL;
    int l_JSONResponseLength =
        asprintf( &l_JSONResponse, "{\"response\": \"%s\"}", l_response );

    free( l_response );

    if ( ( l_JSONResponseLength < 0 ) || ( !l_JSONResponse ) ) {
        mg_printf( _connection,
                   "HTTP/1.1 500 Internal Server Error\r\n"
                   "Content-Length: 0\r\n\r\n" );

        free( l_JSONResponse );

        return ( 500 );
    }

    mg_printf( _connection,
               "HTTP/1.1 200 OK\r\n"
               "Content-Type: application/json\r\n"
               "Content-Length: %d\r\n\r\n"
               "%s",
               l_JSONResponseLength, l_JSONResponse );

    free( l_JSONResponse );

    return ( 200 );
}

int main( void ) {
    if ( !openLLMChatExecutable() ) {
        log( "Failed to open " LLM_CHAT_EXECUTABLE_NAME "\n" );

        return ( 1 );
    }

    const char* l_serverOptions[] = { "document_root", "./" SERVE_DIRECTORY,
                                      "listening_ports", SERVER_PORT, NULL };

    struct mg_context* l_serverContext = NULL;

    if ( !initServer( &l_serverContext, l_serverOptions ) ) {
        return ( 1 );
    }

    // Register URI handler
    mg_set_request_handler( l_serverContext, "/generate", handleGenerate,
                            NULL );

    printf( "Server started on port 8080. Press enter to quit.\n" );
    getchar();

    if ( !quitServer( l_serverContext ) ) {
        return ( 1 );
    }

EXIT:
    return ( 0 );
}
