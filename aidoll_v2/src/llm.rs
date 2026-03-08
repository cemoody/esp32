use crate::conversation::ConversationTurn;
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Message {
    pub role: String,
    pub content: String,
}

/// Parse a single SSE line and extract the content token.
/// Returns None if the line is not a data line, has no content, or content is empty.
pub fn parse_sse_token(line: &str) -> Option<String> {
    let line = line.trim();
    if !line.starts_with("data: ") {
        return None;
    }
    let data = &line[6..];
    if data == "[DONE]" {
        return None;
    }

    // Find "content":" and extract until unescaped closing quote
    let marker = "\"content\":\"";
    let idx = data.find(marker)?;
    let start = idx + marker.len();
    let bytes = data.as_bytes();

    // Scan for unescaped closing quote
    let mut end = start;
    while end < bytes.len() {
        if bytes[end] == b'"' && (end == start || bytes[end - 1] != b'\\') {
            break;
        }
        end += 1;
    }
    if end >= bytes.len() || end == start {
        return None;
    }

    let raw = &data[start..end];
    if raw.is_empty() {
        return None;
    }

    // Unescape JSON sequences
    let mut result = String::with_capacity(raw.len());
    let chars: Vec<char> = raw.chars().collect();
    let mut i = 0;
    while i < chars.len() {
        if chars[i] == '\\' && i + 1 < chars.len() {
            match chars[i + 1] {
                '"' => { result.push('"'); i += 2; }
                '\\' => { result.push('\\'); i += 2; }
                'n' => { result.push('\n'); i += 2; }
                't' => { result.push('\t'); i += 2; }
                _ => { result.push(chars[i]); i += 1; }
            }
        } else {
            result.push(chars[i]);
            i += 1;
        }
    }

    Some(result)
}

/// Returns true if the SSE line signals end of stream.
pub fn is_sse_done(line: &str) -> bool {
    line.trim() == "data: [DONE]"
}

/// Build the messages array for a chat completion request.
pub fn build_messages(
    system_prompt: &str,
    history: &[ConversationTurn],
    user_msg: &str,
) -> Vec<Message> {
    let mut messages = Vec::with_capacity(2 + history.len());

    messages.push(Message {
        role: "system".to_string(),
        content: system_prompt.to_string(),
    });

    for turn in history {
        messages.push(Message {
            role: turn.role.clone(),
            content: turn.content.clone(),
        });
    }

    messages.push(Message {
        role: "user".to_string(),
        content: user_msg.to_string(),
    });

    messages
}
