# PolyphaseResize
AviSynth filter for scaling pixel-perfect sources

Currently only scales horizontally, both passes at once is planned.

Example usage (assuming 256x224 clip resolution):

```
LoadPlugin("PolyphaseResize.dll")
AviSource("clip.avi")
ConvertToRGB32()
PolyphaseResize(1440, 224)
TurnLeft()
PolyphaseResize(1080, 1440)
TurnRight()
```
