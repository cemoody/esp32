use std::path::Path;

/// Load a character prompt from a markdown file.
pub fn load_prompt(path: &Path) -> String {
    std::fs::read_to_string(path).unwrap_or_else(|e| {
        log::warn!("Failed to load prompt from {:?}: {}", path, e);
        String::new()
    })
}
