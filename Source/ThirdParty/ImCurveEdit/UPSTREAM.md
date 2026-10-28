Source: https://github.com/CedricGuillemet/ImGuizmo
Commit: 105e4fde45e4e74aae8973d98750820ef7aca7e2
Files: src/ImCurveEdit.h, src/ImCurveEdit.cpp, LICENSE
License: MIT (see LICENSE)

Local change: reset widget interaction state when the graph ID changes. This prevents
stale selected point indices when switching tracks/channels or adding/deleting keys.
Sequence integration and graph labels live in SequencerCurveEditor.cpp.
