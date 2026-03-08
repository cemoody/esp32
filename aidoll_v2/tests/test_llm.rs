use aidoll_v2::conversation::ConversationTurn;
use aidoll_v2::llm::{build_messages, is_sse_done, parse_sse_token};

#[test]
fn parse_sse_token_basic() {
    let line = r#"data: {"choices":[{"delta":{"content":"Hello"}}]}"#;
    assert_eq!(parse_sse_token(line), Some("Hello".to_string()));
}

#[test]
fn parse_sse_empty_content_skipped() {
    let line = r#"data: {"choices":[{"delta":{"role":"assistant","content":""}}]}"#;
    assert_eq!(parse_sse_token(line), None);
}

#[test]
fn parse_sse_escaped_quotes() {
    let line = r#"data: {"choices":[{"delta":{"content":"He said \"hello\""}}]}"#;
    assert_eq!(
        parse_sse_token(line),
        Some(r#"He said "hello""#.to_string())
    );
}

#[test]
fn parse_sse_done_signal() {
    assert!(is_sse_done("data: [DONE]"));
    assert!(!is_sse_done("data: {\"choices\":[]}"));
    assert!(!is_sse_done("164"));
}

#[test]
fn parse_sse_non_data_line_ignored() {
    assert_eq!(parse_sse_token("164"), None);
    assert_eq!(parse_sse_token(""), None);
    assert_eq!(parse_sse_token("\r"), None);
}

#[test]
fn build_messages_with_history() {
    let history = vec![
        ConversationTurn {
            role: "user".into(),
            content: "Hi".into(),
        },
        ConversationTurn {
            role: "assistant".into(),
            content: "Hello!".into(),
        },
    ];
    let msgs = build_messages("You are a bot.", &history, "How are you?");

    assert_eq!(msgs.len(), 4); // system + 2 history + user
    assert_eq!(msgs[0].role, "system");
    assert_eq!(msgs[0].content, "You are a bot.");
    assert_eq!(msgs[1].role, "user");
    assert_eq!(msgs[1].content, "Hi");
    assert_eq!(msgs[2].role, "assistant");
    assert_eq!(msgs[3].role, "user");
    assert_eq!(msgs[3].content, "How are you?");
}

#[test]
fn build_messages_no_history() {
    let msgs = build_messages("System prompt.", &[], "Hello");
    assert_eq!(msgs.len(), 2);
    assert_eq!(msgs[0].role, "system");
    assert_eq!(msgs[1].role, "user");
    assert_eq!(msgs[1].content, "Hello");
}
