"""
OpenSUP - PGS subtitle encoder.

Top-level package that re-exports metadata (version, author,
contributors, copyright, license) for external consumption.
"""

from opensup.__metadata__ import __version__, __author__, __contributors__, __copyright__, __license__

__all__ = ["__version__", "__author__", "__contributors__", "__copyright__", "__license__"]
