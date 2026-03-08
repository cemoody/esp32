use aidoll_v2::tts::{build_flush_message, build_speak_message, parse_tts_message, TtsEvent};

#[test]
fn detect_flushed_event() {
    let json = r#"{"type":"Flushed"}"#;
    assert_eq!(parse_tts_message(json), Some(TtsEvent::Flushed));
}

#[test]
fn ignore_non_flushed_metadata() {
    let json = r#"{"type":"Warning","message":"something"}"#;
    assert_eq!(parse_tts_message(json), None);
}

#[test]
fn speak_message_format() {
    let msg = build_speak_message("Hello world");
    let parsed: serde_json::Value = serde_json::from_str(&msg).unwrap();
    assert_eq!(parsed["type"], "Speak");
    assert_eq!(parsed["text"], "Hello world");
}

#[test]
fn flush_message_format() {
    let msg = build_flush_message();
    let parsed: serde_json::Value = serde_json::from_str(&msg).unwrap();
    assert_eq!(parsed["type"], "Flush");
}
