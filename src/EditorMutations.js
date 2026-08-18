.pragma library

function normalizePlainText(text) {
    return text.replace(/\r\n/g, "\u2029").replace(/[\r\n]/g, "\u2029");
}

function toggleWrap(editor, before, after) {
    var start = Math.min(editor.selectionStart, editor.selectionEnd);
    var end = Math.max(editor.selectionStart, editor.selectionEnd);
    var selected = editor.getText(start, end);

    // Selection includes the markers: strip them.
    if (selected.length >= before.length + after.length
            && selected.startsWith(before) && selected.endsWith(after)) {
        var inner = selected.slice(before.length, selected.length - after.length);
        replaceRange(editor, start, end, inner, 0, inner.length);
        return;
    }

    // Wrap the selected inner text.
    replaceRange(editor, start, end,
                 before + selected + after,
                 before.length,
                 before.length + selected.length);
}

function replaceRange(editor, rangeStart, rangeEnd, replacement,
                      selectionStartOffset, selectionEndOffset) {
    var start = Math.max(0, Math.min(editor.length, rangeStart));
    var end = Math.max(start, Math.min(editor.length, rangeEnd));
    var insertedText = normalizePlainText(replacement);

    if (start !== end)
        editor.remove(start, end);

    editor.cursorPosition = start;
    editor.insert(start, insertedText);

    // TextEdit.insert() already leaves the caret after the inserted text. Only
    // move it again when the caller deliberately requests a selection/caret
    // within the replacement.
    if (selectionStartOffset !== undefined && selectionEndOffset !== undefined) {
        var insertedEnd = editor.cursorPosition;
        var selectionStart = Math.max(start,
                                      Math.min(insertedEnd, start + selectionStartOffset));
        var selectionEnd = Math.max(start,
                                    Math.min(insertedEnd, start + selectionEndOffset));
        if (selectionStart === selectionEnd)
            editor.cursorPosition = selectionStart;
        else
            editor.select(selectionStart, selectionEnd);
    }

    return insertedText;
}
