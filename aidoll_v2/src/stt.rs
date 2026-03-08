use serde_json::Value;

#[derive(Debug, PartialEq)]
pub enum SttEvent {
    PartialTranscript(String),
    FinalTranscript(String),
    UtteranceEnd,
}

/// Parse a Deepgram STT JSON response into an SttEvent.
/// Returns None if the message should be ignored (empty transcript, unknown type).
pub fn parse_deepgram_message(json: &str) -> Option<SttEvent> {
    let v: Value = serde_json::from_str(json).ok()?;

    let msg_type = v["type"].as_str()?;

    match msg_type {
        "UtteranceEnd" => Some(SttEvent::UtteranceEnd),
        "Results" => {
            let transcript = v["channel"]["alternatives"][0]["transcript"].as_str()?;
            if transcript.is_empty() {
                return None;
            }

            let is_final = v["is_final"].as_bool().unwrap_or(false);
            let speech_final = v["speech_final"].as_bool().unwrap_or(false);

            if is_final || speech_final {
                Some(SttEvent::FinalTranscript(transcript.to_string()))
            } else {
                Some(SttEvent::PartialTranscript(transcript.to_string()))
            }
        }
        _ => None,
    }
}
