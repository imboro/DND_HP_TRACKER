#pragma once
// Undo / redo over whole-board snapshots (the same text as the save file), so
// every kind of change (HP, moves, adds, edits, deletes) can be undone the same way.
//
// The two stacks are private: the rest of the app can only push a state, undo or
// redo, so it can never leave them out of step (e.g. forget to clear redo).
#include <cstddef>
#include <string>
#include <vector>

class UndoHistory {
public:
    // Remembers `state` as the one to go back to; a new change clears redo.
    void push(const std::string& state);

    bool canUndo() const { return !undoStack.empty(); }
    bool canRedo() const { return !redoStack.empty(); }
    std::size_t undoCount() const { return undoStack.size(); }
    std::size_t redoCount() const { return redoStack.size(); }

    // Returns the state to go back to (call only when canUndo()); `current` is kept
    // so redo can return to it.
    std::string undo(const std::string& current);
    // The opposite of undo (call only when canRedo()).
    std::string redo(const std::string& current);

    void clear();

private:
    static constexpr std::size_t LIMIT = 100;  // oldest steps are dropped beyond this
    std::vector<std::string> undoStack;
    std::vector<std::string> redoStack;
};
