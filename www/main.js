const chatBox = document.getElementById( "chat" );

function appendMessage( who, text ) {
    const message = document.createElement( "div" );

    message.className =
        ( ( who === "user" ) ? ( "self-end bg-blue-600" )
                             : ( "self-start bg-gray-700" ) ) +
        " break-words font-medium leading-relaxed max-w-[70%] px-5 py-3 rounded-xl shadow text-[1.125rem] text-white whitespace-pre-line z-10";
    message.textContent = `${text}`;

    chatBox.appendChild( message );

    chatBox.scrollTop = chatBox.scrollHeight;
}

document.getElementById( "chat-form" )
    .addEventListener( "submit", ( event ) => {
        event.preventDefault();

        const promptInput = document.getElementById( "prompt" );
        const prompt = promptInput.value.trim().replaceAll( "\"", "'" );

        if ( !prompt ) {
            return;
        }

        appendMessage( "user", prompt );
        promptInput.value = "";

        fetch( "/generate", {
            method : "POST",
            headers : { "Content-Type" : "application/json" },
            body : JSON.stringify( { prompt } ),
        } )
            .then( ( response ) => response.text() )
            .then(
                ( text ) =>
                    text.replace( /<\|im_start\|>[^\n]*\n|<\|im_end\|>/g, '' )
                        .replaceAll( "\n", "__NEW_LINE__" )
                        .replace( /\s+/g, " " ) )
            .then( ( data ) => {
                appendMessage( "bot", JSON.parse( data ).response.replaceAll(
                                          "__NEW_LINE__", "\n" ) );
            } )
            .catch( ( error ) => {
                appendMessage( "bot", "Error: " + error.message );
            } );
    } );
