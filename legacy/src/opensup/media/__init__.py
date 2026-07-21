"""
Media processing modules.

Handles palette manipulation, image quantization (multiple backends
including libimagequant, Pillow, HexTree, Qtzr), PGS graphics object
construction, and packetised stream assembly.

Why this separation: quantisation is the most tunned part of the
pipeline and benefits from being isolated so backends can be tested
and swapped independently.
"""
