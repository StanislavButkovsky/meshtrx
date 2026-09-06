"""Общая память бота и MCP-сервера: сообщения, очередь ответов, вопросы.

Демон и MCP-сервер — разные процессы, и связывает их эта база. Так сделано не
из любви к базам: длинный опрос Telegram может вести только один процесс, иначе
два клиента начинают отбирать обновления друг у друга. Демон остаётся
единственным, кто говорит с Telegram, а всё остальное общается через SQLite.
"""

from __future__ import annotations

import json
import os
import sqlite3
import time
from pathlib import Path

SCHEMA = """
CREATE TABLE IF NOT EXISTS messages (
    id            INTEGER PRIMARY KEY,          -- message_id из Telegram
    chat_id       INTEGER NOT NULL,
    chat_title    TEXT,
    user_id       INTEGER,
    user_name     TEXT,
    text          TEXT,
    reply_to      INTEGER,
    is_command    INTEGER DEFAULT 0,
    ts            REAL NOT NULL,
    seen          INTEGER DEFAULT 0,            -- прочитано ли агентом
    media_path    TEXT                          -- скачанная картинка, если была
);

CREATE TABLE IF NOT EXISTS outbox (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    chat_id       INTEGER NOT NULL,
    text          TEXT NOT NULL,
    reply_to      INTEGER,
    created_ts    REAL NOT NULL,
    sent_ts       REAL,
    error         TEXT
);

CREATE TABLE IF NOT EXISTS questions (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    message_id    INTEGER NOT NULL,
    chat_id       INTEGER NOT NULL,
    user_name     TEXT,
    text          TEXT NOT NULL,
    created_ts    REAL NOT NULL,
    answered_ts   REAL,
    answer        TEXT
);

CREATE TABLE IF NOT EXISTS state (
    key           TEXT PRIMARY KEY,
    value         TEXT
);

CREATE INDEX IF NOT EXISTS idx_messages_ts ON messages(ts);
CREATE INDEX IF NOT EXISTS idx_outbox_unsent ON outbox(sent_ts);
"""


def data_dir() -> Path:
    base = os.environ.get("XDG_DATA_HOME") or (Path.home() / ".local" / "share")
    path = Path(base) / "meshtrx"
    path.mkdir(parents=True, exist_ok=True)
    return path


def db_path() -> Path:
    return Path(os.environ.get("MESHTRX_TG_DB", data_dir() / "telegram.db"))


def connect() -> sqlite3.Connection:
    conn = sqlite3.connect(db_path(), timeout=10)
    conn.row_factory = sqlite3.Row
    # Журнал с упреждающей записью: демон пишет, MCP-сервер читает, и без него
    # читатель блокировал бы писателя на каждом обновлении.
    conn.execute("PRAGMA journal_mode=WAL")
    conn.executescript(SCHEMA)
    # База на сервере создана раньше, чем появились картинки, а CREATE TABLE
    # IF NOT EXISTS существующую таблицу не трогает — недостающий столбец
    # добавляем руками, иначе демон упадёт на первом же фото.
    have = {row["name"] for row in conn.execute("PRAGMA table_info(messages)")}
    if "media_path" not in have:
        conn.execute("ALTER TABLE messages ADD COLUMN media_path TEXT")
        conn.commit()
    return conn


# ---------------------------------------------------------------- сообщения

def save_message(conn: sqlite3.Connection, msg: dict, chat: dict, user: dict,
                 is_command: bool = False, media_path: str | None = None) -> None:
    # У картинки текста нет, зато бывает подпись — и раньше терялась и она:
    # в базу ложилась пустая строка, а человек был уверен, что показал главное.
    text = msg.get("text") or msg.get("caption") or ""
    conn.execute(
        "INSERT OR REPLACE INTO messages"
        " (id, chat_id, chat_title, user_id, user_name, text, reply_to, is_command, ts, seen,"
        "  media_path)"
        " VALUES (?,?,?,?,?,?,?,?,?, COALESCE((SELECT seen FROM messages WHERE id=?), 0), ?)",
        (msg.get("message_id"), chat.get("id"), chat.get("title") or chat.get("username"),
         user.get("id"), (user.get("username") or user.get("first_name") or "?"),
         text, (msg.get("reply_to_message") or {}).get("message_id"),
         1 if is_command else 0, msg.get("date", time.time()), msg.get("message_id"),
         media_path))
    conn.commit()


def recent_messages(conn: sqlite3.Connection, limit: int = 30,
                    only_unseen: bool = False, chat_id: int | None = None) -> list[sqlite3.Row]:
    sql = "SELECT * FROM messages"
    where, args = [], []
    if only_unseen:
        where.append("seen = 0")
    if chat_id is not None:
        where.append("chat_id = ?")
        args.append(chat_id)
    if where:
        sql += " WHERE " + " AND ".join(where)
    sql += " ORDER BY ts DESC LIMIT ?"
    args.append(limit)
    rows = conn.execute(sql, args).fetchall()
    return list(reversed(rows))


def mark_seen(conn: sqlite3.Connection, ids: list[int]) -> None:
    if not ids:
        return
    conn.executemany("UPDATE messages SET seen = 1 WHERE id = ?", [(i,) for i in ids])
    conn.commit()


# ---------------------------------------------------------------- отправка

def queue_message(conn: sqlite3.Connection, chat_id: int, text: str,
                  reply_to: int | None = None) -> int:
    cur = conn.execute(
        "INSERT INTO outbox (chat_id, text, reply_to, created_ts) VALUES (?,?,?,?)",
        (chat_id, text, reply_to, time.time()))
    conn.commit()
    return int(cur.lastrowid)


def pending_outbox(conn: sqlite3.Connection, limit: int = 20) -> list[sqlite3.Row]:
    return conn.execute(
        "SELECT * FROM outbox WHERE sent_ts IS NULL ORDER BY id LIMIT ?", (limit,)).fetchall()


def mark_sent(conn: sqlite3.Connection, row_id: int, error: str | None = None) -> None:
    conn.execute("UPDATE outbox SET sent_ts = ?, error = ? WHERE id = ?",
                 (time.time(), error, row_id))
    conn.commit()


# ---------------------------------------------------------------- вопросы

def add_question(conn: sqlite3.Connection, message_id: int, chat_id: int,
                 user_name: str, text: str) -> int:
    cur = conn.execute(
        "INSERT INTO questions (message_id, chat_id, user_name, text, created_ts)"
        " VALUES (?,?,?,?,?)", (message_id, chat_id, user_name, text, time.time()))
    conn.commit()
    return int(cur.lastrowid)


def open_questions(conn: sqlite3.Connection, limit: int = 20) -> list[sqlite3.Row]:
    return conn.execute(
        "SELECT * FROM questions WHERE answered_ts IS NULL ORDER BY id LIMIT ?",
        (limit,)).fetchall()


def close_question(conn: sqlite3.Connection, qid: int, answer: str) -> None:
    conn.execute("UPDATE questions SET answered_ts = ?, answer = ? WHERE id = ?",
                 (time.time(), answer, qid))
    conn.commit()


# ---------------------------------------------------------------- состояние

def get_state(conn: sqlite3.Connection, key: str, default=None):
    row = conn.execute("SELECT value FROM state WHERE key = ?", (key,)).fetchone()
    return json.loads(row["value"]) if row else default


def set_state(conn: sqlite3.Connection, key: str, value) -> None:
    conn.execute("INSERT OR REPLACE INTO state (key, value) VALUES (?,?)",
                 (key, json.dumps(value, ensure_ascii=False)))
    conn.commit()
