.pragma library

function normalizePlainText(text) {
    return text.replace(/\r\n/g, "\u2029").replace(/[\r\n]/g, "\u2029");
}

// The rich-text editor reads inserted text as HTML, which drops leading and
// repeated spaces. Text holding such runs goes in as a whitespace-preserving span.
function insertableText(text) {
    if (!/^[ \t]|[ \t]{2}|\t|\u2029[ \t]/.test(text))
        return text;
    return "<span style=\"white-space: pre-wrap\">"
        + text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
        + "</span>";
}

// With nothing selected, `span` describes the emphasis around the caret: at the
// end of its text the caret steps past the closing marker (and back in on a
// second press), anywhere else inside it the emphasis comes off.
function toggleWrap(editor, before, after, span) {
    var start = Math.min(editor.selectionStart, editor.selectionEnd);
    var end = Math.max(editor.selectionStart, editor.selectionEnd);
    if (start === end && span && span.start !== undefined) {
        if (start === span.end) {
            editor.cursorPosition = span.textEnd;
        } else if (start >= span.textEnd) {
            editor.cursorPosition = span.end;
        } else {
            var content = editor.getText(span.contentStart, span.contentEnd);
            var caret = Math.max(0, start - span.contentStart);
            replaceRange(editor, span.start, span.end, content, caret, caret);
        }
        return;
    }
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
    editor.insert(start, insertableText(insertedText));

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
