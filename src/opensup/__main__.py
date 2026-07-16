"""
Entry point for ``python -m opensup``.

Dispatches to CLI (when arguments are provided) or GUI (when
called with no arguments). This dual-mode entry point lets the
package work both as a command-line tool and as a desktop application
from a single install.
"""
import sys


def main():
    if len(sys.argv) > 1:
        from opensup.cli import main as cli_main
        sys.exit(cli_main())
    else:
        from opensup.gui import main as gui_main
        gui_main()


if __name__ == '__main__':
    main()
