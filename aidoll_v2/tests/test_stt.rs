use aidoll_v2::stt::{parse_deepgram_message, SttEvent};

#[test]
fn parse_final_transcript() {
    let json = r#"{"type":"Results","is_final":true,"speech_final":true,"channel":{"alternatives":[{"transcript":"Hello Harry"}]}}"#;
    assert_eq!(
        parse_deepgram_message(json),
        Some(SttEvent::FinalTranscript("Hello Harry".to_string()))
    );
}

#[test]
fn parse_partial_transcript() {
    let json = r#"{"type":"Results","is_final":false,"speech_final":false,"channel":{"alternatives":[{"transcript":"Hello"}]}}"#;
    assert_eq!(
        parse_deepgram_message(json),
        Some(SttEvent::PartialTranscript("Hello".to_string()))
    );
}

#[test]
fn ignore_utterance_end() {
    let json = r#"{"type":"UtteranceEnd"}"#;
    assert_eq!(parse_deepgram_message(json), Some(SttEvent::UtteranceEnd));
}

#[test]
fn empty_transcript_skipped() {
    let json = r#"{"type":"Results","is_final":true,"speech_final":true,"channel":{"alternatives":[{"transcript":""}]}}"#;
    assert_eq!(parse_deepgram_message(json), None);
}

#[test]
fn parse_real_deepgram_response() {
    let json = r#"{"type":"Results","channel_index":[0,1],"duration":1.04,"start":3.36,"is_final":true,"speech_final":true,"channel":{"alternatives":[{"transcript":"Hello there.","confidence":0.99}]}}"#;
    assert_eq!(
        parse_deepgram_message(json),
        Some(SttEvent::FinalTranscript("Hello there.".to_string()))
    );
}
