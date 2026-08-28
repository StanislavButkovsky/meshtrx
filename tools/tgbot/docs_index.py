"""Индекс документации проекта для быстрых ответов бота.

Никаких моделей и векторов: документация проекта — это десяток файлов, и
обычный поиск по словам с ранжированием по заголовкам справляется лучше, чем
кажется. Задача индекса — не заменить разбор вопроса, а мгновенно отдать
готовый кусок руководства, когда человек спрашивает то, что там написано.

Если ничего похожего не нашлось, бот честно молчит и оставляет вопрос в
очереди — на такой отвечает человек или агент, читающий репозиторий целиком.
"""

from __future__ import annotations

import math
import re
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PULL_TIMEOUT = 30      # секунд на git pull: сеть до GitHub бывает медленной
SOURCES = [
    "docs/USER_GUIDE.md",
    "docs/ROADMAP.md",
    "README.md",
    "desktop/README.md",
    "firmware/test/harness/README.md",
    "MESHTRX_SPEC.md",
]
# Спрашивают люди с устройством в руках, поэтому руководство пользователя
# весит больше, чем спецификация: на «как подключить ретранслятор» нужен
# раздел про кнопку и меню, а не описание repeater.cpp. Технические документы
# не выброшены — они отвечают, когда в руководстве такого нет вовсе.
SOURCE_WEIGHT = {
    "docs/USER_GUIDE.md": 1.6,
    "docs/ROADMAP.md": 1.2,
    "README.md": 1.0,
    "desktop/README.md": 1.0,
    "MESHTRX_SPEC.md": 0.5,
    "firmware/test/harness/README.md": 0.4,
}
# Порог подобран по живым вопросам: верные попадания дают 17 и выше, ложные
# срабатывания — около десяти. Лучше промолчать и оставить вопрос человеку,
# чем ответить уверенно и мимо: неверный ответ дороже отсутствия ответа.
MIN_SCORE = 14         # ниже — считаем, что не нашли
MAX_ANSWER = 1200      # символов: длиннее в чате уже не читают
SITE_DOCS = "https://meshtrx.com/docs"

# Слова, которые есть в каждом втором вопросе и ни на что не указывают. Без
# этого списка вопрос «как подключить ретранслятор» находил раздел «Как
# вызвать»: одно слово «как» в заголовке весит втрое и перебивает совпадение
# по существу.
STOPWORDS = {
    "как", "что", "где", "когда", "почему", "зачем", "чем", "кто", "какой",
    "какая", "какие", "какое", "можно", "нужно", "надо", "есть", "быть",
    "это", "этот", "эта", "для", "при", "или", "если", "тут", "там", "так",
    "все", "всё", "уже", "ещё", "еще", "мне", "меня", "него", "них", "нет",
    "да", "не", "бот", "меш", "сделать", "работает", "делать",
}

# Люди пишут не теми словами, что в документации: «блютуз» против «BLE»,
# «рация» против «устройство», «позвонить» против «вызов». Словарь приводит их
# к одному виду — иначе самый частый вопрос группы «не видит по блютузу» не
# находит ничего.
SYNONYMS = {
    "блютуз": "ble", "блютус": "ble", "bluetooth": "ble", "блутуз": "ble",
    "рация": "устройств", "рацию": "устройств", "нода": "устройств",
    "ноду": "устройств", "девайс": "устройств", "станция": "устройств",
    "плата": "устройств", "плату": "устройств",
    "звонить": "вызов", "позвонить": "вызов", "звонок": "вызов",
    "вызвать": "вызов", "созвониться": "вызов",
    "репитер": "ретранслятор", "repeater": "ретранслятор",
    "телефон": "телефон", "смартфон": "телефон", "андроид": "android",
    "апк": "apk", "приложуха": "приложени", "программа": "приложени",
    "коннект": "подключени", "подключить": "подключени",
    "подключение": "подключени", "соединение": "подключени",
    "прошить": "прошивк", "перепрошить": "прошивк", "firmware": "прошивк",
    "громкость": "громкост", "микрофон": "микрофон",
    "лора": "lora", "лоры": "lora", "лоре": "lora", "радио": "lora",
}

# Приставки окончаний, которые режем, чтобы «ретранслятор» находил
# «ретранслятора», а «прошивку» — «прошивка». Полноценная морфология тут
# не нужна: словарь маленький, а библиотеку пришлось бы тащить на сервер.
ENDINGS = ("ями", "ами", "ого", "ему", "ому", "ых", "их", "ый", "ий", "ой",
           "ая", "яя", "ое", "ее", "ые", "ие", "ам", "ям", "ах", "ях", "ов",
           "ев", "ей", "ью", "ом", "ем", "ах", "у", "ю", "а", "я", "ы", "и",
           "о", "е", "ь")


# Не всякое обращение — вопрос по документации. «Привет» и «спасибо» незачем
# записывать в очередь и обещать разобраться: человек ждал двух слов в ответ,
# а получил канцелярскую расписку. А «как всё работает» состоит из одних
# стоп-слов, и после отсева от вопроса не остаётся ничего — искать нечего,
# но ответить есть что.
INTENTS: list[tuple[str, str]] = [
    (r"^\W*(привет|здравствуй|здрасьте|добрый (день|вечер|утро)|доброе утро|"
     r"hi|hello|хай)\W*$",
     "Привет. Спрашивайте про MeshTRX — про связь, прошивку, приложение или "
     "устройство. Например: /ask сколько можно говорить."),

    (r"^\W*(спасибо|спс|благодарю|thanks|thx|пасибо|понял|ясно)\W*$",
     "Пожалуйста. Если что-то не заработает — пишите сюда."),

    (r"(как\s+(это|всё|все|оно)\s+работает|что\s+(это|такое)\s*(meshtrx|за проект)?|"
     r"расскажи\s+о\s+проекте|что\s+умеет|как\s+устроено)",
     "MeshTRX — голосовая связь без сотовой сети и интернета. Два устройства "
     "Heltec с радиомодулем LoRa держат связь между собой на несколько "
     "километров, а к каждому по Bluetooth подключается телефон или "
     "компьютер: через них идут голос, сообщения, файлы и вызовы. Есть режим "
     "ретранслятора, чтобы дотянуться дальше.\n\n"
     "Подробно — https://meshtrx.com/docs"),
]


def quick_answer(question: str) -> str | None:
    """Готовый ответ на обращение, которое не стоит искать в документации."""
    text = question.strip().lower()
    for pattern, answer in INTENTS:
        if re.search(pattern, text):
            return answer
    return None


def is_searchable(question: str) -> bool:
    """Осталось ли в вопросе хоть одно значащее слово. «Как все работает?»
    целиком состоит из стоп-слов — искать по нему нечего."""
    return bool(_normalize(question))


@dataclass
class Section:
    source: str
    title: str
    body: str

    @property
    def text(self) -> str:
        return f"{self.title}\n{self.body}"


def _stem(word: str) -> str:
    """Грубая основа слова: отрезаем окончание, если слово от этого не
    рассыплется. Пять букв — порог, ниже которого резать опасно: «сеть»
    превратилась бы в «се»."""
    for end in ENDINGS:
        if len(word) - len(end) >= 5 and word.endswith(end):
            return word[:-len(end)]
    return word


def _normalize(text: str) -> list[str]:
    words = re.split(r"[^\wёЁ-]+", text.lower())
    out = []
    for w in words:
        if len(w) <= 2 or w in STOPWORDS:
            continue
        # Синоним ищем и по самому слову, и по его основе: в вопросе бывает
        # «блютузу», в словаре — «блютуз».
        w = SYNONYMS.get(w) or SYNONYMS.get(_stem(w)) or _stem(w)
        out.append(w)
    return out


def _plain(text: str) -> str:
    """Убирает разметку Markdown: в чат она приезжает как есть, и человек
    читает «нажмите кнопку **ОБЩИЙ**» вместе со звёздочками.

    Отправлять с parse_mode нельзя: в документации попадаются символы, на
    которых разбор падает, и тогда сообщение не уходит вовсе — молча.
    """
    text = re.sub(r"^\s{0,3}#{1,6}\s*", "", text, flags=re.M)     # заголовки
    text = re.sub(r"\*\*(.+?)\*\*", r"\1", text, flags=re.S)      # жирный
    text = re.sub(r"(?<!\w)[*_](.+?)[*_](?!\w)", r"\1", text)     # курсив
    text = re.sub(r"`{1,3}", "", text)                            # код
    text = re.sub(r"\[(.+?)\]\((.+?)\)", r"\1 (\2)", text)        # ссылки
    text = re.sub(r"^\s*[-*]\s+", "• ", text, flags=re.M)         # списки
    text = re.sub(r"^\s*\|.*\|\s*$", lambda m: _row(m.group(0)),
                  text, flags=re.M)                               # таблицы
    text = re.sub(r"\n{3,}", "\n\n", text)
    return text.strip()


def _row(line: str) -> str:
    """Строка таблицы в чате нечитаема, а разделитель `|---|---|` и вовсе
    мусор. Разворачиваем в «ячейка — ячейка»."""
    cells = [c.strip() for c in line.strip().strip("|").split("|")]
    if all(set(c) <= set("-: ") for c in cells):
        return ""
    return " — ".join(c for c in cells if c)


class DocsIndex:
    def __init__(self, root: Path | None = None):
        self.root = root or ROOT
        self.sections: list[Section] = []
        self.df: dict[str, int] = {}      # в скольких разделах встречается слово
        self.updated: float = 0.0
        self.revision: str = ""
        self.rebuild()

    # -------------------------------------------------------- обновление
    def pull(self) -> tuple[bool, str]:
        """Подтягивает документацию из репозитория.

        Источник правды — GitHub, а не копия на сервере: копию пришлось бы
        досылать после каждой правки, и однажды бот начал бы отвечать
        позавчерашним руководством, ничем этого не выдав.

        Только ускоренная перемотка: если на сервере оказались свои изменения,
        честнее упасть с ошибкой, чем молча их затереть.
        """
        try:
            before = self._revision()
            subprocess.run(["git", "-C", str(self.root), "pull", "--ff-only", "--quiet"],
                           check=True, capture_output=True, timeout=PULL_TIMEOUT)
            after = self._revision()
            return after != before, after
        except FileNotFoundError:
            return False, "git не установлен"
        except subprocess.TimeoutExpired:
            return False, "GitHub не ответил вовремя"
        except subprocess.CalledProcessError as e:
            err = (e.stderr or b"").decode(errors="replace").strip().splitlines()
            return False, err[-1] if err else "git отказался обновляться"

    def _revision(self) -> str:
        try:
            out = subprocess.run(["git", "-C", str(self.root), "rev-parse", "--short", "HEAD"],
                                 check=True, capture_output=True, timeout=10)
            return out.stdout.decode().strip()
        except (OSError, subprocess.SubprocessError):
            return ""

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
            trail: list[str] = []          # путь заголовков до текущего
            for part in parts:
                lines = part.strip().split("\n")
                if not lines:
                    continue
                head = lines[0]
                level = len(head) - len(head.lstrip("#")) or 1
                title = head.lstrip("# ").strip()
                body = "\n".join(lines[1:]).strip()

                # Путь заголовков — то, чего не хватало больше всего.
                # Подраздел «Включение» сам по себе не говорит ни о чём, и на
                # вопрос про ретранслятор не находился; в паре с родителем —
                # «Режим ретранслятора › Включение» — находится сразу.
                trail = trail[:level - 1] + [title]

                if len(body) < 40:
                    continue
                # Отсылки вида «См. раздел такой-то» — оглавление, а не ответ.
                if len(body) < 150 and re.search(r"см\.?\s+раздел", body, re.I):
                    continue
                # Первый уровень — название самого документа: в ответе оно
                # только занимает строку («MeshTRX — Руководство пользователя ›
                # …»), поэтому в заголовок не идёт.
                self.sections.append(Section(rel, " › ".join(trail[1:] or trail), body))
        # Насколько слово вообще редкое. Без этого «ble» из вопроса «не видит
        # по блютузу» весит столько же, сколько «видит», хотя встречается в
        # трети разделов — и побеждал тот раздел, что раньше в файле.
        self.df = {}
        for section in self.sections:
            for w in set(_normalize(section.text)):
                self.df[w] = self.df.get(w, 0) + 1

        self.updated = time.time()
        self.revision = self._revision()
        return self.status()

    def _idf(self, word: str) -> float:
        return math.log(1 + len(self.sections) / (1 + self.df.get(word, 0)))

    def status(self) -> dict:
        return {
            "files": len({s.source for s in self.sections}),
            "sections": len(self.sections),
            "size": sum(len(s.body) for s in self.sections),
            "updated": time.strftime("%d.%m.%Y %H:%M", time.localtime(self.updated)),
            "revision": self.revision,
        }

    # ------------------------------------------------------------- поиск
    def search(self, question: str) -> str | None:
        words = set(_normalize(question))
        if not words:
            return None

        best, best_score = None, 0.0
        for section in self.sections:
            title_words = set(_normalize(section.title))
            body = _normalize(section.body)
            body_words = set(body)
            score = 0.0
            for w in words:
                weight = self._idf(w)
                # Заголовок весит вчетверо: раздел «Режим ретранслятора»
                # отвечает на вопрос про ретранслятор лучше, чем раздел, где
                # это слово упомянуто вскользь.
                if w in title_words:
                    score += 4 * weight
                if w in body_words:
                    # Плюс за то, что слово встречается часто, а не один раз в
                    # примечании. Потолок в три — чтобы длинный раздел не
                    # выигрывал одним своим размером.
                    score += weight * (1 + min(body.count(w), 3))
            score *= SOURCE_WEIGHT.get(section.source, 1.0)
            if score > best_score:
                best, best_score = section, score

        if not best or best_score < MIN_SCORE:
            return None

        body = _plain(best.body)
        if len(body) > MAX_ANSWER:
            body = body[:MAX_ANSWER].rsplit("\n", 1)[0] + "\n…"
        # Ссылку даём на сайт, а не на файл в репозитории: «docs/USER_GUIDE.md»
        # человеку в чате не говорит ничего — непонятно, где это искать.
        return f"{_plain(best.title)}\n\n{body}\n\nПолное руководство — {SITE_DOCS}"
