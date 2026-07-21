#!/usr/bin/env python3
"""OpenSUP - PGS Subtitle Encoder.
Usage:  python main.py            (GUI)
        python main.py -i input.xml output.sup  (CLI)

No pip install required - this script adds src/ to sys.path.
"""
import os
import sys
import traceback
import multiprocessing


def _hide_console() -> None:
    """Hide the console window on Windows (PyInstaller frozen mode)."""
    if os.name == 'nt' and getattr(sys, 'frozen', False):
        try:
            import ctypes
            ctypes.windll.user32.ShowWindow(
                ctypes.windll.kernel32.GetConsoleWindow(), 0)
        except Exception:
            pass


# Allow running without pip install (adds src/ to path)
src_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'src')
if os.path.isdir(src_dir):
    sys.path.insert(0, src_dir)

if __name__ == '__main__':
    multiprocessing.freeze_support()
    try:
        if len(sys.argv) > 1:
            print("OpenSUP - PGS Subtitle Encoder (CLI mode)")
            from opensup.cli import main as cli_main  # pyright: ignore[reportMissingImports]
            sys.exit(cli_main())
        else:
            print("OpenSUP - PGS Subtitle Encoder")
            print("Starting application...")
            from opensup.gui import main as gui_main  # pyright: ignore[reportMissingImports]
            gui_main()
    except ModuleNotFoundError as e:
        print(f"\nERROR: Missing dependency: {e}")
        print("Install with: pip install numpy pillow opencv-python customtkinter")
    except Exception:
        print("\nERROR durante la inicialización:")
        traceback.print_exc()
    finally:
        # Si llegamos aquí con error o la GUI se cerró, mantener consola abierta
        if getattr(sys, 'frozen', False) or not os.isatty(0):
            print("\nPresione Enter para salir...")
            try:
                input()
            except (EOFError, KeyboardInterrupt):
                pass
        sys.exit(1)
