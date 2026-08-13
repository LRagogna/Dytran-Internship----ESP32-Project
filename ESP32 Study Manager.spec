# -*- mode: python ; coding: utf-8 -*-
#
# PyInstaller spec for ESP32 Study Manager.
#
# Build with:   pyinstaller "ESP32 Study Manager.spec"
#
# This is the repeatable source of truth for the build. Prefer editing and
# rebuilding from this file over retyping the command line.

from PyInstaller.utils.hooks import collect_all

# Collect data files, binaries, and hidden imports for packages that
# PyInstaller does not fully discover on its own.
datas = [("web", "web")]
binaries = []
hiddenimports = []

for pkg in ("webview", "pdfplumber", "pdfminer"):
    pkg_datas, pkg_binaries, pkg_hiddenimports = collect_all(pkg)
    datas += pkg_datas
    binaries += pkg_binaries
    hiddenimports += pkg_hiddenimports


block_cipher = None


a = Analysis(
    ["app.py"],
    pathex=[],
    binaries=binaries,
    datas=datas,
    hiddenimports=hiddenimports,
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    cipher=block_cipher,
    noarchive=False,
)

pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.zipfiles,
    a.datas,
    [],
    name="ESP32 Study Manager",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=False,          # windowed app, no console
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    # icon="app.ico",       # uncomment and provide an .ico to set an icon
)
