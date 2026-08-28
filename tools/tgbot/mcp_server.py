#!/usr/bin/env python3
"""MCP-сервер: даёт агенту читать группу MeshTRX в Telegram и отвечать в неё.

С Telegram сервер не разговаривает вовсе — он читает и пишет ту же базу, что
и демон. Причина простая: длинный опрос отдаёт обновление ровно одному
клиенту, и два процесса начали бы отбирать сообщения друг у друга. Здесь же
чтение переписки не мешает боту работать, а отправка переживает перезапуск
сессии: сообщение лежит в очереди, пока демон его не отправит.

Регистрируется в .mcp.json проекта; запускается Claude Code по stdio.
"""

from __future__ import annotations

import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import store                                  # noqa: E402
from docs_index import DocsIndex              # noqa: E402
from mcp.server.mcpserver import MCPServer     # noqa: E402

mcp = MCPServer("meshtrx-telegram")
_docs = DocsIndex()

PERSONA_FILE = Path(__file__).resolve().parent / "persona.md"


def _persona() -> str:
    """Правила тона лежат в файле, а не в коде: их правят чаще, чем логику, и
    правит их обычно не программист."""
    try:
        return PERSONA_FILE.read_text(encoding="utf-8").strip()
    except OSError:
        return ""


def _default_chat() -> int | None:
    """Чат по умолчанию: тот, что указан в настройках, иначе последний, из
    которого приходили сообщения. Второе спасает, когда идентификатор группы
    ещё не вписан в файл настроек — обычная ситуация в первый день."""
    from daemon import load_env
    env = load_env()
    raw = (env.get("MESHTRX_TG_CHAT_ID") or "").split(",")[0].strip()
    if raw:
        try:
            return int(raw)
        except ValueError:
            pass
    conn = store.connect()
    row = conn.execute("SELECT chat_id FROM messages ORDER BY ts DESC LIMIT 1").fetchone()
    return int(row["chat_id"]) if row else None


def _format(rows) -> str:
    if not rows:
        return "новых сообщений нет"
    lines = []
    for r in rows:
        stamp = time.strftime("%d.%m %H:%M", time.localtime(r["ts"]))
        mark = " (команда)" if r["is_command"] else ""
        reply = f" ↩{r['reply_to']}" if r["reply_to"] else ""
        lines.append(f"[{r['id']}] {stamp} {r['user_name']}{mark}{reply}: {r['text']}")
    return "\n".join(lines)


@mcp.tool()
def telegram_read(limit: int = 30, only_new: bool = True, mark_read: bool = True) -> str:
    """Прочитать сообщения группы MeshTRX.

    limit — сколько последних показать; only_new — только непрочитанные;
    mark_read — пометить показанное прочитанным, чтобы в следующий раз
    пришло только новое.
    """
    conn = store.connect()
    rows = store.recent_messages(conn, limit=limit, only_unseen=only_new)
    if mark_read and rows:
        store.mark_seen(conn, [r["id"] for r in rows])
    return _format(rows)


@mcp.tool()
def telegram_send(text: str, reply_to: int | None = None,
                  chat_id: int | None = None) -> str:
    """Отправить сообщение в группу.

    Пишите как участник команды: коротко, обычным языком, без подписей вроде
    «— бот MeshTRX» и без служебных формул. Полные правила — telegram_style,
    их же напоминает ответ этого инструмента.

    Сообщение кладётся в очередь, демон отправит его в ближайшую секунду.
    reply_to — идентификатор сообщения, на которое отвечаем (номер в
    квадратных скобках из telegram_read).
    """
    target = chat_id or _default_chat()
    if target is None:
        return ("не знаю, куда отправлять: укажите chat_id или впишите "
                "MESHTRX_TG_CHAT_ID в ~/.config/meshtrx/telegram.env")
    conn = store.connect()
    row_id = store.queue_message(conn, target, text, reply_to)
    return f"поставлено в очередь (#{row_id}) для чата {target}"


@mcp.tool()
def telegram_questions() -> str:
    """Вопросы, заданные боту командой /ask, на которые ещё нет ответа."""
    conn = store.connect()
    rows = store.open_questions(conn)
    if not rows:
        return "неотвеченных вопросов нет"
    return "\n".join(
        f"#{r['id']} от {r['user_name']} ({time.strftime('%d.%m %H:%M', time.localtime(r['created_ts']))}): "
        f"{r['text']}" for r in rows)


@mcp.tool()
def telegram_answer(question_id: int, answer: str) -> str:
    """Ответить на вопрос из telegram_questions: ответ уходит в тот же чат
    ответом на исходное сообщение, а вопрос закрывается.

    Тон — как в telegram_style: живой язык, без подписи и без канцелярита."""
    conn = store.connect()
    row = conn.execute("SELECT * FROM questions WHERE id = ?", (question_id,)).fetchone()
    if row is None:
        return f"вопроса #{question_id} нет"
    if row["answered_ts"]:
        return f"вопрос #{question_id} уже закрыт"
    store.queue_message(conn, row["chat_id"], answer, row["message_id"])
    store.close_question(conn, question_id, answer)
    return f"ответ на #{question_id} поставлен в очередь"


@mcp.tool()
def telegram_chats() -> str:
    """Чаты, из которых боту приходили сообщения, с их идентификаторами.

    Нужен, когда группа только заведена: у групп идентификатор отрицательный,
    у супергрупп начинается с -100, а положительное число — это личный чат.
    """
    conn = store.connect()
    rows = conn.execute(
        "SELECT chat_id, chat_title, COUNT(*) n, MAX(ts) last"
        " FROM messages GROUP BY chat_id ORDER BY last DESC").fetchall()
    if not rows:
        return "сообщений ещё не было — напишите что-нибудь в группе"
    lines = []
    for r in rows:
        kind = "группа" if r["chat_id"] < 0 else "личный чат"
        stamp = time.strftime("%d.%m %H:%M", time.localtime(r["last"]))
        lines.append(f"{r['chat_id']}  {kind}  «{r['chat_title']}»  "
                     f"сообщений {r['n']}, последнее {stamp}")
    return "\n".join(lines)


@mcp.tool()
def telegram_status() -> str:
    """Состояние: сколько сообщений в базе, что не отправлено, жив ли демон."""
    conn = store.connect()
    total = conn.execute("SELECT COUNT(*) c FROM messages").fetchone()["c"]
    unseen = conn.execute("SELECT COUNT(*) c FROM messages WHERE seen = 0").fetchone()["c"]
    pending = len(store.pending_outbox(conn))
    questions = len(store.open_questions(conn))
    last = conn.execute("SELECT ts FROM messages ORDER BY ts DESC LIMIT 1").fetchone()
    last_txt = (time.strftime("%d.%m %H:%M", time.localtime(last["ts"]))
                if last else "сообщений ещё не было")
    offset = store.get_state(conn, "offset", 0)
    info = _docs.status()
    return (f"сообщений {total}, непрочитанных {unseen}, в очереди на отправку {pending}, "
            f"открытых вопросов {questions}\nпоследнее сообщение: {last_txt}\n"
            f"позиция опроса Telegram: {offset}\n"
            f"индекс документации: {info['files']} файлов, {info['sections']} разделов, "
            f"версия {info['revision'] or '?'}, собран {info['updated']}\n"
            f"база: {store.db_path()}")


@mcp.tool()
def telegram_style() -> str:
    """Как писать в группу: тон, чего не делать, примеры.

    Прочитайте перед первым сообщением в сессии. Коротко: пишем как участник
    команды, не подписываемся ботом, не разводим канцелярит; но если человек
    прямо спросил, кто отвечает, — отвечаем честно.
    """
    return _persona() or "файл tools/tgbot/persona.md не найден"


@mcp.tool()
def telegram_docs_search(question: str) -> str:
    """Найти ответ в документации проекта — тем же поиском, что у бота.
    Полезно, чтобы ответить человеку теми же словами, что он увидит от /ask."""
    found = _docs.search(question)
    return found or "в документации ничего похожего не нашлось"


@mcp.tool()
def telegram_docs_reload() -> str:
    """Подтянуть документацию из репозитория и пересобрать индекс.

    Сам он тоже обновляется раз в четверть часа — этот инструмент нужен, когда
    правку запушили только что и ответ по ней нужен немедленно.
    """
    changed, rev = _docs.pull()
    info = _docs.rebuild()
    state = f"обновлено до {rev}" if changed else f"без изменений ({rev or 'версия неизвестна'})"
    return (f"{state}: {info['files']} файлов, {info['sections']} разделов, "
            f"{info['size'] // 1024} КБ")


if __name__ == "__main__":
    mcp.run()
