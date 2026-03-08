#[cfg(feature = "esp32")]
pub mod client {
    use crate::conversation::ConversationTurn;
    use crate::llm::{build_messages, is_sse_done, parse_sse_token};
    use anyhow::Result;
    use embedded_svc::http::client::Client as HttpClient;
    use embedded_svc::io::Read;
    use esp_idf_svc::http::client::{Configuration, EspHttpConnection};

    /// Make a streaming chat completion request to Groq.
    /// Calls `on_token` for each content token, returns the full response.
    pub fn complete(
        api_key: &str,
        host: &str,
        model: &str,
        max_tokens: u32,
        system_prompt: &str,
        history: &[ConversationTurn],
        user_msg: &str,
        mut on_token: impl FnMut(&str),
    ) -> Result<String> {
        let messages = build_messages(system_prompt, history, user_msg);

        let body = serde_json::json!({
            "model": model,
            "max_tokens": max_tokens,
            "stream": true,
            "temperature": 0.7,
            "messages": messages,
        })
        .to_string();

        let config = Configuration {
            crt_bundle_attach: Some(esp_idf_svc::sys::esp_crt_bundle_attach),
            ..Default::default()
        };

        let mut client = HttpClient::wrap(EspHttpConnection::new(&config)?);

        let url = format!("https://{}/openai/v1/chat/completions", host);
        let auth = format!("Bearer {}", api_key);
        let content_len = body.len().to_string();

        let headers = [
            ("content-type", "application/json"),
            ("authorization", auth.as_str()),
            ("content-length", content_len.as_str()),
        ];

        let mut request = client.post(&url, &headers)?;

        use embedded_svc::io::Write;
        request.write_all(body.as_bytes())?;
        request.flush()?;

        let mut response = request.submit()?;

        if response.status() != 200 {
            let mut err_buf = [0u8; 512];
            let n = response.read(&mut err_buf).unwrap_or(0);
            let err_text = std::str::from_utf8(&err_buf[..n]).unwrap_or("unknown error");
            anyhow::bail!("[GROQ] HTTP {}: {}", response.status(), err_text);
        }

        // Read SSE stream
        let mut full_response = String::new();
        let mut line_buf = String::new();
        let mut byte_buf = [0u8; 1];

        loop {
            match response.read(&mut byte_buf) {
                Ok(0) => break,
                Ok(1) => {
                    let ch = byte_buf[0] as char;
                    line_buf.push(ch);

                    if ch == '\n' {
                        let line = line_buf.trim().to_string();
                        line_buf.clear();

                        if is_sse_done(&line) {
                            break;
                        }

                        if let Some(token) = parse_sse_token(&line) {
                            full_response.push_str(&token);
                            on_token(&token);
                        }
                    }
                }
                Ok(_) => {}
                Err(_) => break,
            }
        }

        Ok(full_response)
    }
}
