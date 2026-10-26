# Rail warning signal

Source: Kenney City Kit Roads 2.2, https://kenney.nl/assets/city-kit-roads
Downloaded from the official Kenney site. CC0 terms are preserved in License.txt.

- traffic-light.obj / traffic-light.mtl / Textures/colormap.png: original pole and signal housing.
- crossy-rail-signal-off.obj / crossy-rail-signal-on.obj: 2.4-unit-high variants; red lens switches between dark and bright red, green/amber lenses remain dark.
- crossy-rail-signal.mtl: palette-backed housing and separate lens colors; no emissive shader dependency.
- Tools/BuildCrossySignals.mjs rebuilds both variants from the original OBJ.

LevelManager.lua faces the signal toward -X and mounts a red point light beside its red lens.
Signals blink from 2 seconds before departure for at most 3 seconds in total, then all switch off together. MapTrainBlinkDuration controls this duration independently of the pre-departure warning time. Lights cast no shadows and are disabled with recycled rows.
