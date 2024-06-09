# PolyphaseResize
AviSynth filter for scaling pixel-perfect sources. Requires a RGB32 source.

Example source clips could include NES/SNES footage at 256x224, Game Boy footage at 160x144, etc.

Usage:
```
LoadPlugin("PolyphaseResize.dll")
AviSource("clip.avi")
ConvertToRGB32()
PolyphaseResize(1440, 1080)
```
