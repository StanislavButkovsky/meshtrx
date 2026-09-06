#!/usr/bin/env python3
"""Проверка синтаксиса страницы ретранслятора — до прошивки, а не после.

Страница собирается в прошивке склейкой строковых литералов, и одна лишняя
кавычка ломает весь скрипт разом: страница открывается, а на ней ничего не
работает — ни радар, ни список станций. Именно так и случилось: `style='…'`
внутри строки в одинарных кавычках. Заметить это можно было только на
телефоне, подключённом к точке доступа ретранслятора, — то есть дорого.

Скрипт вытаскивает литералы из handleMap(), склеивает и отдаёт получившийся
JavaScript на проверку node. Работает за секунду и не требует железа.

    python3 firmware/test/check_web_page.py
"""

import pathlib
import re
import shutil
import subprocess
import sys

SRC = pathlib.Path(__file__).resolve().parents[1] / "src" / "wifi_monitor.cpp"


def extract() -> str:
    src = SRC.read_text(encoding="utf-8")
    start = src.index("static void handleMap()")
    end = src.index("// Станции в виде JSON")
    literals = re.findall(r'"((?:[^"\\]|\\.)*)"', src[start:end])
    return "".join(literals).encode().decode("unicode_escape")


def main() -> int:
    html = extract()
    if "<script>" not in html:
        print("страница собралась без скрипта — проверять нечего", file=sys.stderr)
        return 1
    js = html.split("<script>")[1].split("</script>")[0]

    node = shutil.which("node")
    if not node:
        print("node не найден — проверка пропущена")
        return 0

    tmp = pathlib.Path("/tmp/meshtrx-repeater-page.js")
    tmp.write_text(js, encoding="utf-8")
    r = subprocess.run([node, "--check", str(tmp)], capture_output=True, text=True)
    tmp.unlink(missing_ok=True)
    if r.returncode == 0:
        print(f"страница ретранслятора в порядке: {len(html)} байт, скрипт {len(js)} байт")
        return 0
    # Node печатает и строку с ошибкой, и её позицию — этого достаточно
    print("в скрипте страницы ошибка:", file=sys.stderr)
    print(r.stderr.strip()[-600:], file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
