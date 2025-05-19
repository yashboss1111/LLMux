#include <limits.h>
#include <llama.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "log.h"

// Configuration
#define MODELS_DIRECTORY "models"
#define MODEL_NAME "llama-2-7b-chat.Q2_K"
#define MODEL_PATH MODELS_DIRECTORY "/" MODEL_NAME ".gguf"
#define GPU_LAYERS 30
#define CONTEXT_SIZE 2048
#define ANNULED_CONTEXT_MESSAGE "Context was annulled. Try again."

static std::string trim( const std::string& _input ) {
    size_t l_startIndex = 0;

    while ( ( l_startIndex < _input.size() ) &&
            ( std::isspace(
                static_cast< unsigned char >( _input[ l_startIndex ] ) ) ) ) {
        ++l_startIndex;
    }

    size_t l_endIndex = _input.size();

    while ( ( l_endIndex > l_startIndex ) &&
            ( std::isspace(
                static_cast< unsigned char >( _input[ l_endIndex - 1 ] ) ) ) ) {
        --l_endIndex;
    }

    return ( _input.substr( l_startIndex, l_endIndex - l_startIndex ) );
}

static void truncate( char** _string, const ssize_t _from, const ssize_t _to ) {
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
            truncate( &l_directoryPath, -1, l_lastSlashIndex );

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

int main( void ) {
    // Only print errors
    llama_log_set(
        []( enum ggml_log_level _level, const char* _text, void* _data ) {
            if ( _level >= GGML_LOG_LEVEL_ERROR ) {
                fprintf( stderr, "%s", _text );
            }
        },
        nullptr );

    // Load dynamic backends
    ggml_backend_load_all();

    // Initialize the model
    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = GPU_LAYERS;

    char* modelPath = getApplicationDirectoryAbsolutePath();

    concatBeforeAndAfterString( &modelPath, NULL, MODEL_PATH );

    llama_model* model = llama_model_load_from_file( modelPath, model_params );

    free( modelPath );

    if ( !model ) {
        log( "Unable to load model\n" );

        return ( 1 );
    }

    const llama_vocab* vocab = llama_model_get_vocab( model );

    // Initialize the context
    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = CONTEXT_SIZE;
    ctx_params.n_batch = CONTEXT_SIZE;

    llama_context* ctx = llama_init_from_model( model, ctx_params );

    if ( !ctx ) {
        log( "Failed to create the llama_context\n" );

        return ( 1 );
    }

    // Initialize the sampler
    llama_sampler* smpl =
        llama_sampler_chain_init( llama_sampler_chain_default_params() );
    llama_sampler_chain_add( smpl, llama_sampler_init_min_p( 0.05f, 1 ) );
    llama_sampler_chain_add( smpl, llama_sampler_init_temp( 0.8f ) );
    llama_sampler_chain_add( smpl,
                             llama_sampler_init_dist( LLAMA_DEFAULT_SEED ) );

    // Helper function to evaluate a prompt and generate a response
    auto generate = [ & ]( const std::string& prompt ) {
        std::string response;

        const bool is_first = llama_kv_self_used_cells( ctx ) == 0;

        // Tokenize the prompt
        const int n_prompt_tokens = -llama_tokenize(
            vocab, prompt.c_str(), prompt.size(), NULL, 0, is_first, true );

        std::vector< llama_token > prompt_tokens( n_prompt_tokens );

        if ( llama_tokenize( vocab, prompt.c_str(), prompt.size(),
                             prompt_tokens.data(), prompt_tokens.size(),
                             is_first, true ) < 0 ) {
            GGML_ABORT( "Failed to tokenize the prompt\n" );
        }

        // Prepare a batch for the prompt
        llama_batch batch =
            llama_batch_get_one( prompt_tokens.data(), prompt_tokens.size() );
        llama_token new_token_id;

        while ( true ) {
            // Check if we have enough space in the context to evaluate this
            // batch
            int n_ctx = llama_n_ctx( ctx );
            int n_ctx_used = llama_kv_self_used_cells( ctx );

            if ( n_ctx_used + batch.n_tokens > n_ctx ) {
                response = ANNULED_CONTEXT_MESSAGE;

                break;
            }

            if ( llama_decode( ctx, batch ) ) {
                GGML_ABORT( "Failed to decode\n" );
            }

            // Sample the next token
            new_token_id = llama_sampler_sample( smpl, ctx, -1 );

            // is it an end of generation?
            if ( llama_vocab_is_eog( vocab, new_token_id ) ) {
                break;
            }

            // Convert the token to a string, print it and add it to the
            // response
            char buf[ 256 ];
            int n = llama_token_to_piece( vocab, new_token_id, buf,
                                          sizeof( buf ), 0, true );

            if ( n < 0 ) {
                GGML_ABORT( "Failed to convert token to piece\n" );
            }

            std::string piece( buf, n );
            response += piece;

            // Prepare the next batch with the sampled token
            batch = llama_batch_get_one( &new_token_id, 1 );
        }

        return ( response );
    };

    std::vector< llama_chat_message > messages;
    std::vector< char > formatted( llama_n_ctx( ctx ) );
    int prev_len = 0;

    while ( true ) {
        // get user input
        std::string user;
        std::getline( std::cin, user );

        if ( user.empty() ) {
            break;
        }

        const char* tmpl =
            llama_model_chat_template( model, /* name */ nullptr );

        // Add the user input to the message list and format it
        messages.push_back( { "user", strdup( user.c_str() ) } );

        int new_len = llama_chat_apply_template(
            tmpl, messages.data(), messages.size(), true, formatted.data(),
            formatted.size() );

        if ( new_len > ( int )formatted.size() ) {
            formatted.resize( new_len );

            new_len = llama_chat_apply_template(
                tmpl, messages.data(), messages.size(), true, formatted.data(),
                formatted.size() );
        }

        if ( new_len < 0 ) {
            log( "Failed to apply the chat template\n" );

            return ( 1 );
        }

        // Remove previous messages to obtain the prompt to generate the
        // response
        std::string prompt( formatted.begin() + prev_len,
                            formatted.begin() + new_len );

        // Generate a response
        log( "Prompt: %s\n", prompt.c_str() );

        std::string response = generate( prompt );

        if ( !response.empty() ) {
            response = trim( response );

            log( "Response: %s\n", response.c_str() );

            printf( "%s\n", response.c_str() );

            fflush( stdout );

            if ( response == ANNULED_CONTEXT_MESSAGE ) {
                // Skip pushing it into messages, don’t add assistant message
                prev_len = 0;

                // Clear the KV cache so the model forgets everything:
                llama_kv_self_clear( ctx );

                messages.clear();

                continue;
            }
        }

        // Add the response to the messages
        messages.push_back( { "assistant", strdup( response.c_str() ) } );

        prev_len = llama_chat_apply_template(
            tmpl, messages.data(), messages.size(), false, nullptr, 0 );

        if ( prev_len < 0 ) {
            log( "Failed to apply the chat template\n" );

            return ( 1 );
        }
    }

    // Free resources
    for ( auto& msg : messages ) {
        free( const_cast< char* >( msg.content ) );
    }

    llama_sampler_free( smpl );
    llama_free( ctx );
    llama_model_free( model );

    return ( 0 );
}
