"""Индекс документации проекта для быстрых ответов бота.

Никаких моделей и векторов: документация проекта — это десяток файлов, и
обычный поиск по словам с ранжированием по заголовкам справляется лучше, чем
кажется. Задача индекса — не заменить разбор вопроса, а мгновенно отдать
готовый кусок руководства, когда человек спрашивает то, что там написано.

Если ничего похожего не нашлось, бот честно молчит и оставляет вопрос в
очереди — на такой отвечает человек или агент, читающий репозиторий целиком.
"""

from __future__ import annotations

import re
import time
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCES = [
    "docs/USER_GUIDE.md",
    "docs/ROADMAP.md",
    "README.md",
    "desktop/README.md",
    "firmware/test/harness/README.md",
    "MESHTRX_SPEC.md",
]
MIN_SCORE = 3          # ниже — считаем, что не нашли
MAX_ANSWER = 1200      # символов: длиннее в чате уже не читают


@dataclass
class Section:
    source: str
    title: str
    body: str

    @property
    def text(self) -> str:
        return f"{self.title}\n{self.body}"


def _normalize(text: str) -> list[str]:
    return [w for w in re.split(r"[^\wёЁ-]+", text.lower()) if len(w) > 2]


class DocsIndex:
    def __init__(self, root: Path | None = None):
        self.root = root or ROOT
        self.sections: list[Section] = []
        self.updated: float = 0.0
        self.rebuild()

    # ------------------------------------------------------------- сборка
    def rebuild(self) -> dict:
        self.sections = []
        for rel in SOURCES:
            path = self.root / rel
            if not path.exists():
                continue
            text = path.read_text(encoding="utf-8", errors="replace")
            # Режем по заголовкам: раздел руководства — естественная единица
            # ответа, человек получает связный кусок, а не обрывок строки.
            parts = re.split(r"\n(?=#{1,3} )", text)
            for part in parts:
                lines = part.strip().split("\n")
                if not lines:
                    continue
                title = lines[0].lstrip("# ").strip()
                body = "\n".join(lines[1:]).strip()
                if len(body) < 40:
                    continue
                self.sections.append(Section(rel, title, body))
        self.updated = time.time()
        return self.status()

    def status(self) -> dict:
        return {
            "files": len({s.source for s in self.sections}),
            "sections": len(self.sections),
            "size": sum(len(s.body) for s in self.sections),
            "updated": time.strftime("%d.%m.%Y %H:%M", time.localtime(self.updated)),
        }

    # ------------------------------------------------------------- поиск
    def search(self, question: str) -> str | None:
        words = set(_normalize(question))
        if not words:
            return None

        best, best_score = None, 0
        for section in self.sections:
            title_words = set(_normalize(section.title))
            body_words = set(_normalize(section.body))
            # Совпадение в заголовке весит втрое: раздел «Ограничение речи»
            # отвечает на вопрос про длительность лучше, чем случайное
            # упоминание слова «речь» в середине спецификации.
            score = 3 * len(words & title_words) + len(words & body_words)
            if score > best_score:
                best, best_score = section, score

        if not best or best_score < MIN_SCORE:
            return None

        body = best.body
        if len(body) > MAX_ANSWER:
            body = body[:MAX_ANSWER].rsplit("\n", 1)[0] + "\n…"
        # Ссылку на файл даём как подсказку «где почитать целиком», а не как
        # служебную подпись: человеку полезно, а на подпись не похоже.
        return f"{best.title}\n\n{body}\n\nПодробнее — {best.source}"
