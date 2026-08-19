#!/usr/bin/env python3
"""Запуск клиента из исходников: python3 desktop/run.py"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from meshtrx_desktop.app import main  # noqa: E402

sys.exit(main())
