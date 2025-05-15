#include <civetweb.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"

#define LLM_CHAT_EXECUTABLE_NAME "llm_chat"
#define SERVE_DIRECTORY "www"
#define SERVER_PORT "8080"

int to_child[ 2 ];   // Parent writes to child
int from_child[ 2 ]; // Parent reads from child

static void trim( char** _string, const ssize_t _from, const ssize_t _to ) {
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
            trim( &l_directoryPath, -1, l_lastSlashIndex );

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

        pipe( to_child );
        pipe( from_child );

        pid_t pid = fork();

        if ( pid == 0 ) {
            // CHILD PROCESS
            dup2( to_child[ 0 ], STDIN_FILENO );    // stdin - pipe read end
            dup2( from_child[ 1 ], STDOUT_FILENO ); // stdout - pipe write end

            // Close unused ends
            close( to_child[ 1 ] );
            close( from_child[ 0 ] );

            execlp( l_executablePath, l_executablePath, NULL );

            log( "execlp failed\n" );

            l_returnValue = false;

        } else {
            // PARENT PROCESS
            // Close unused ends
            close( to_child[ 0 ] );
            close( from_child[ 1 ] );
        }

        free( l_executablePath );

        l_returnValue = true;
    }

EXIT:
    return ( l_returnValue );
}

static char* llm_generate( const char* _prompt ) {
    // Write to child
    const char* msg = "how to say hello?\n";
    write( to_child[ 1 ], msg, strlen( msg ) );

    // Read from child
    char buffer[ 2024 ];
    ssize_t n = read( from_child[ 0 ], buffer, sizeof( buffer ) - 1 );

    if ( n > 0 ) {
        buffer[ n ] = '\0';

        printf( "Child said: %s", buffer );
    }

    return ( strdup( buffer ) );
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

static int handleGenerate( struct mg_connection* _connection, void* _cbdata ) {
    char buf[ 8192 ];
    int len = mg_read( _connection, buf, sizeof( buf ) );
    buf[ len ] = '\0';

    // Very simple JSON parse: look for "prompt"
    char* p = strstr( buf, "\"prompt\"" );

    if ( !p ) {
        mg_printf( _connection,
                   "HTTP/1.1 400 Bad Request\r\n"
                   "Content-Type: text/plain\r\n"
                   "Content-Length: 11\r\n\r\n"
                   "Bad Request" );

        return ( 400 );
    }

    // Extract prompt value ( naive )
    p = strchr( p, ':' );
    p = strchr( p, '"' ) + 1;
    char* end = strchr( p, '"' );
    size_t prompt_len = end - p;
    char prompt[ 1024 ] = { 0 };
    strncpy(
        prompt, p,
        prompt_len < sizeof( prompt ) - 1 ? prompt_len : sizeof( prompt ) - 1 );

    // Generate response
    char* reply = llm_generate( prompt );

    if ( !reply ) {
        mg_printf( _connection,
                   "HTTP/1.1 400 Bad Request\r\n"
                   "Content-Type: text/plain\r\n"
                   "Content-Length: 11\r\n\r\n"
                   "Bad Request" );

        return ( 400 );
    }

    // Build JSON response
    char response[ 8192 ];
    int rlen = snprintf( response, sizeof( response ), "{\"response\": \"%s\"}",
                         reply );
    free( reply );

    // Send HTTP response
    mg_printf( _connection,
               "HTTP/1.1 200 OK\r\n"
               "Content-Type: application/json\r\n"
               "Content-Length: %d\r\n\r\n"
               "%s",
               rlen, response );

    return 200;
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
