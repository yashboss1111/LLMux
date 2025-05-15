const chatBox = document.getElementById( 'chat' );
const promptInput = document.getElementById( 'prompt' );
const sendBtn = document.getElementById( 'send' );

function appendMessage( who, text ) {
    const msg = document.createElement( 'div' );
    msg.className =
        ( who === 'user' ? 'self-end bg-blue-600' : 'self-start bg-gray-700' ) +
        ' text-white text-[1.125rem] leading-relaxed font-medium px-5 py-3 rounded-xl shadow max-w-[70%] break-words';
    msg.textContent = `${text}`;
    chatBox.appendChild( msg );
    chatBox.scrollTop = chatBox.scrollHeight;
}

document.getElementById( 'chat-form' ).addEventListener( 'submit', ( e ) => {
    e.preventDefault();

    const input = document.getElementById( 'prompt' );
    const chat = document.getElementById( 'chat' );
    const prompt = input.value.trim();
    if ( !prompt )
        return;

    appendMessage( 'user', prompt );
    input.value = '';

    fetch( '/generate', {
        method : 'POST',
        headers : { 'Content-Type' : 'application/json' },
        body : JSON.stringify( { prompt } ),
    } )
        .then( (res) => {
            return (res.json());
        })
        .then( (data) => {
            appendMessage( 'bot', data.response );
        })
        .catch( (err) => {
            appendMessage( 'bot', 'Error: ' + err.message );
        });
} );
