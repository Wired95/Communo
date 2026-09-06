
-- Table structure
CREATE TABLE chat_rooms (
    id              INTEGER PRIMARY KEY,
    name            TEXT NOT NULL,
    password_hash   BLOB,

    CHECK (id >= 0 AND id <= 255),
    CHECK (password_hash IS NULL OR length(password_hash) = 32)
);

-- Table values
-- The password for the private room is "Password"
INSERT INTO chat_rooms (id, name, password_hash) VALUES
    (1, 'General',    NULL),
    (2, 'Fun',        NULL),
    (3, 'Private',    X'e7cf3ef4f17c3999a94f2c6f612e8a888e5b1026878e4e19398b23bd38ec221a');
