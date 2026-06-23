/* Jupiter port: stub editor.h replacing upstream (real one pulls in widgets.h
 * and the level-editor machinery, which we don't build for v1).
 *
 * Game logic only reads Editor.Running and Editor.State. Editor is never
 * actually "running" in a player build. */
#ifndef JUPITER_EDITOR_STUB_H
#define JUPITER_EDITOR_STUB_H

enum EditorRunningType {
    EditorNotRunning  = 0,
    EditorStarted     = 1,
    EditorCommandLine = 2,
    EditorEditing     = 3
};

enum class EditorStateType {
    Selecting = 0,
    EditTile,
    EditUnit,
};

class CEditor {
public:
    EditorRunningType Running{EditorNotRunning};
    EditorStateType   State{EditorStateType::Selecting};
    bool Init{false};
};

extern CEditor Editor;

extern void EditorCclRegister();

#endif
