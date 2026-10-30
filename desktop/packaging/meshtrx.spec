# Сборка настольного клиента в одну программу: pyinstaller packaging/meshtrx.spec
# Кодек кладётся внутрь: путь к libcodec2 — в переменной CODEC2_LIB.
import os, sys
from PyInstaller.utils.hooks import collect_submodules

here = os.path.abspath(SPECPATH)
root = os.path.dirname(here)
codec = os.environ.get("CODEC2_LIB", "")
binaries = [(p, ".") for p in codec.split(os.pathsep) if p]

a = Analysis(
    [os.path.join(root, "run.py")],
    pathex=[root],
    binaries=binaries,
    datas=[(os.path.join(root, "meshtrx_desktop", "assets"), "meshtrx_desktop/assets")],
    hiddenimports=collect_submodules("bleak"),
    excludes=["tkinter", "PySide6.QtWebEngineCore", "PySide6.Qt3DCore", "PySide6.QtQuick",
              "PySide6.QtQml", "PySide6.QtMultimedia", "PySide6.QtCharts"],
)
pyz = PYZ(a.pure)
icon = os.path.join(here, "icon.ico" if sys.platform == "win32" else "icon.icns")

if sys.platform == "darwin":
    exe = EXE(pyz, a.scripts, [], exclude_binaries=True, name="MeshTRX",
              console=False, icon=icon)
    coll = COLLECT(exe, a.binaries, a.datas, name="MeshTRX")
    app = BUNDLE(coll, name="MeshTRX.app", icon=icon, bundle_identifier="ru.meshtrx.desktop",
                 info_plist={
                     "NSBluetoothAlwaysUsageDescription":
                         "MeshTRX подключается к рации по Bluetooth.",
                     "NSMicrophoneUsageDescription":
                         "Микрофон нужен, чтобы говорить в эфир.",
                     "CFBundleShortVersionString": os.environ.get("MESHTRX_VERSION", "1.1.0"),
                 })
else:
    exe = EXE(pyz, a.scripts, a.binaries, a.datas, [], name="MeshTRX",
              console=False, icon=icon if sys.platform == "win32" else None,
              upx=False)
