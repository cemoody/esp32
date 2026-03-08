use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct ConversationTurn {
    pub role: String,
    pub content: String,
}

pub struct ConversationHistory {
    turns: Vec<ConversationTurn>,
    max_turns: usize,
}

impl ConversationHistory {
    pub fn new(max_turns: usize) -> Self {
        Self {
            turns: Vec::with_capacity(max_turns),
            max_turns,
        }
    }

    pub fn add(&mut self, role: &str, content: &str) {
        if self.turns.len() >= self.max_turns {
            self.turns.remove(0);
        }
        self.turns.push(ConversationTurn {
            role: role.to_string(),
            content: content.to_string(),
        });
    }

    pub fn turns(&self) -> &[ConversationTurn] {
        &self.turns
    }

    pub fn len(&self) -> usize {
        self.turns.len()
    }

    pub fn clear(&mut self) {
        self.turns.clear();
    }
}
