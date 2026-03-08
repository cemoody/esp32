use serde_json::json;

#[derive(Debug, PartialEq)]
pub enum TtsEvent {
    AudioChunk(Vec<u8>),
    Flushed,
}

/// Parse a Deepgram TTS text (JSON) message.
/// Returns Some(Flushed) for flushed events, None otherwise.
/// Note: audio comes as binary WebSocket frames, not through this function.
pub fn parse_tts_message(json: &str) -> Option<TtsEvent> {
    if json.contains("\"type\":\"Flushed\"") {
        return Some(TtsEvent::Flushed);
    }
    None
}

/// Build the JSON message to send text to Deepgram TTS.
pub fn build_speak_message(text: &str) -> String {
    json!({
        "type": "Speak",
        "text": text
    })
    .to_string()
}

/// Build the JSON flush command.
pub fn build_flush_message() -> String {
    json!({
        "type": "Flush"
    })
    .to_string()
}
