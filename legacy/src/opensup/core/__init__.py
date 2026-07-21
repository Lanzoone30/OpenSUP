"""
Core pipeline modules.

Implements the PGS encoding pipeline: segment construction, bitmap
rendering, file I/O (BDN XML parsing and .sup writing), and the
top-level orchestrator (BDNRender) that ties everything together.

Why this separation: keeps encoding logic independent from the
media/palette layer and the UI, making it reusable from both CLI
and GUI.
"""
