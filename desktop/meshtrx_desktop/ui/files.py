"""Вкладка файлов: отправка через mesh и приём входящих."""

from __future__ import annotations

import os

from PySide6.QtCore import Qt, QThread, Signal
from PySide6.QtWidgets import (QComboBox, QFileDialog, QHBoxLayout, QLabel,
                               QListWidget, QListWidgetItem, QProgressBar,
                               QPushButton, QVBoxLayout, QWidget)

from .. import protocol as proto
from ..client import Client, FileTransfer
from .bridge import Bridge

TYPE_BY_EXT = {
    ".jpg": proto.FileType.PHOTO, ".jpeg": proto.FileType.PHOTO,
    ".png": proto.FileType.PHOTO, ".txt": proto.FileType.TEXT,
    ".wav": proto.FileType.VOICE, ".ogg": proto.FileType.VOICE,
}


class SendWorker(QThread):
    """Загрузка файла в устройство идёт десятками секунд — держим её отдельно
    от интерфейса, иначе окно замирает на всё время передачи."""

    failed = Signal(str)

    def __init__(self, client: Client, path: str, dest: str | None):
        super().__init__()
        self.client, self.path, self.dest = client, path, dest

    def run(self):
        try:
            with open(self.path, "rb") as f:
                data = f.read()
            ext = os.path.splitext(self.path)[1].lower()
            self.client.send_file(os.path.basename(self.path), data,
                                  TYPE_BY_EXT.get(ext, proto.FileType.BINARY),
                                  self.dest)
        except Exception as e:                                  # noqa: BLE001
            self.failed.emit(str(e))


class FilesTab(QWidget):
    def __init__(self, client: Client, bridge: Bridge):
        super().__init__()
        self.client = client
        self._workers: list[SendWorker] = []
        self._rows: dict[int, QListWidgetItem] = {}
        layout = QVBoxLayout(self)

        row = QHBoxLayout()
        row.addWidget(QLabel("Кому:"))
        self.dest = QComboBox(); self.dest.setMinimumWidth(220)
        row.addWidget(self.dest)
        btn = QPushButton("Выбрать файл и отправить"); btn.setProperty("accent", True)
        btn.clicked.connect(self.pick)
        row.addWidget(btn); row.addStretch()
        layout.addLayout(row)

        self.hint = QLabel("Файл сначала загружается в память устройства, "
                           "потом уходит в эфир. Практический потолок — около 65 КБ.")
        self.hint.setProperty("dim", True); self.hint.setWordWrap(True)
        layout.addWidget(self.hint)

        self.list = QListWidget()
        layout.addWidget(self.list, 1)
        self.progress = QProgressBar(); self.progress.setRange(0, 100)
        layout.addWidget(self.progress)

        bridge.transfer.connect(self.on_transfer)
        bridge.file_done.connect(self.on_done)
        bridge.peers_changed.connect(lambda _p: self.refresh_dest())
        self.refresh_dest()

    def refresh_dest(self):
        current = self.dest.currentData()
        self.dest.clear()
        for peer in sorted(self.client.active_peers(), key=lambda p: -p.rssi):
            label = peer.call_sign or f"TX-{peer.device_id[-4:]}"
            self.dest.addItem(f"{label} [{peer.device_id[-4:]}]", peer.device_id[-4:])
        if self.dest.count() == 0:
            self.dest.addItem("нет абонентов в эфире", None)
        idx = self.dest.findData(current)
        if idx >= 0:
            self.dest.setCurrentIndex(idx)

    def pick(self):
        path, _ = QFileDialog.getOpenFileName(self, "Файл для отправки")
        if not path:
            return
        worker = SendWorker(self.client, path, self.dest.currentData())
        worker.failed.connect(lambda e: self.hint.setText(f"Ошибка: {e}"))
        worker.finished.connect(lambda: self._workers.remove(worker)
                                if worker in self._workers else None)
        self._workers.append(worker)
        worker.start()

    def on_transfer(self, tr: FileTransfer):
        key = id(tr)
        arrow = "←" if tr.incoming else "→"
        size_kb = tr.size / 1024
        done = tr.done if not tr.incoming else tr.done
        total = tr.size if not tr.incoming else max(tr.total, 1)
        text = (f"{arrow} {tr.name}  {size_kb:.1f} КБ"
                f"  {'приём' if tr.incoming else 'отправка'} {done}/{total}")
        item = self._rows.get(key)
        if item is None:
            item = QListWidgetItem(text)
            self._rows[key] = item
            self.list.addItem(item)
            self.list.scrollToBottom()
        else:
            item.setText(text)
        if total:
            self.progress.setValue(int(100 * done / total))

    def on_done(self, tr: FileTransfer):
        path, _ = QFileDialog.getSaveFileName(self, "Сохранить принятый файл", tr.name)
        if path:
            with open(path, "wb") as f:
                f.write(bytes(tr.data))
            item = self._rows.get(id(tr))
            if item:
                item.setText(item.text() + f"  → сохранён: {path}")
        self.progress.setValue(100)
