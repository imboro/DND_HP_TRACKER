#include "undo_history.h"

void UndoHistory::push(const std::string& state) {
    undoStack.push_back(state);
    if (undoStack.size() > LIMIT) undoStack.erase(undoStack.begin());
    redoStack.clear();
}

std::string UndoHistory::undo(const std::string& current) {
    redoStack.push_back(current);
    std::string state = undoStack.back();
    undoStack.pop_back();
    return state;
}

std::string UndoHistory::redo(const std::string& current) {
    undoStack.push_back(current);
    std::string state = redoStack.back();
    redoStack.pop_back();
    return state;
}

void UndoHistory::clear() {
    undoStack.clear();
    redoStack.clear();
}
