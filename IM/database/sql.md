```mermaid
erDiagram
    users ||--o{ files : uploads
    users ||--o{ file_transfer_requests : initiates
    users ||--o{ file_transfer_requests : receives
    files ||--o{ file_transfer_requests : "transferred via"
    users ||--o{ personal_messages : sends
    users ||--o{ personal_messages : receives
    users ||--o{ offline_messages : has
    personal_messages ||--o{ offline_messages : "references"

    users {
        BIGINT user_id PK
        VARCHAR(50) username
        VARCHAR(100) password_hash
        VARCHAR(100) email
        VARCHAR(50) nickname
        TINYINT status DEFAULT 0 COMMENT '"0:离线, 1:在线, 2:离开, 3:忙碌"'
        TIMESTAMP create_time DEFAULT CURRENT_TIMESTAMP
        BOOLEAN is_admin DEFAULT FALSE
        BOOLEAN is_approved DEFAULT FALSE COMMENT '"是否已审批通过"'
        INDEX idx_email (email)
        INDEX idx_username (username)
    }

    files {
        BIGINT file_id PK
        VARCHAR(255) file_name
        VARCHAR(255) file_path
        BIGINT file_size
        VARCHAR(50) file_type
        BIGINT uploader_id FK
        TIMESTAMP upload_time
        BOOLEAN is_deleted
    }

    file_transfer_requests {
        BIGINT request_id PK
        BIGINT from_user_id FK
        BIGINT to_user_id FK
        BIGINT file_id FK
        TIMESTAMP create_time
        TINYINT status
        TIMESTAMP complete_time
    }

    personal_messages {
        BIGINT message_id PK
        BIGINT from_user_id FK
        BIGINT to_user_id FK
        TINYINT message_type
        TEXT content
        TEXT extra_info
        TIMESTAMP send_time
        BOOLEAN is_read
    }

    offline_messages {
        BIGINT id PK
        BIGINT message_id FK
        BIGINT user_id FK
        TIMESTAMP create_time
    }