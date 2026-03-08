use aidoll_v2::conversation::ConversationHistory;

#[test]
fn add_single_turn() {
    let mut h = ConversationHistory::new(10);
    h.add("user", "Hello");
    assert_eq!(h.len(), 1);
    assert_eq!(h.turns()[0].role, "user");
    assert_eq!(h.turns()[0].content, "Hello");
}

#[test]
fn add_user_assistant_pair() {
    let mut h = ConversationHistory::new(10);
    h.add("user", "Hi");
    h.add("assistant", "Hello!");
    assert_eq!(h.len(), 2);
    assert_eq!(h.turns()[0].role, "user");
    assert_eq!(h.turns()[1].role, "assistant");
}

#[test]
fn rotation_at_max_turns() {
    let mut h = ConversationHistory::new(3);
    h.add("user", "msg0");
    h.add("assistant", "msg1");
    h.add("user", "msg2");
    assert_eq!(h.len(), 3);

    // Adding a 4th should drop the oldest
    h.add("assistant", "msg3");
    assert_eq!(h.len(), 3);
    assert_eq!(h.turns()[0].content, "msg1");
    assert_eq!(h.turns()[2].content, "msg3");
}

#[test]
fn multiple_overflows() {
    let mut h = ConversationHistory::new(3);
    for i in 0..10 {
        h.add("user", &format!("msg{}", i));
    }
    assert_eq!(h.len(), 3);
    assert_eq!(h.turns()[0].content, "msg7");
    assert_eq!(h.turns()[1].content, "msg8");
    assert_eq!(h.turns()[2].content, "msg9");
}

#[test]
fn roles_preserved() {
    let mut h = ConversationHistory::new(3);
    h.add("user", "a");
    h.add("assistant", "b");
    h.add("user", "c");
    h.add("assistant", "d"); // drops "a"
    assert_eq!(h.turns()[0].role, "assistant");
    assert_eq!(h.turns()[1].role, "user");
    assert_eq!(h.turns()[2].role, "assistant");
}
