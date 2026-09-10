CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME messageformatting-demo
    TITLE "Message Formatting"
    DESCRIPTION "Document-level CSS + syntax-colour theme JSON (Stage 1), live markdown-to-sanitized-HTML rendering with a generated-HTML inspector, sanitization smoke test, and reply-preview strip (Stage 2), code-block syntax highlighting with a language picker across 21+ languages (Stage 3), and a live message editor with a three-value editing mode (WYSIWYG / Markdown / Plaintext), the formatting toolbar behind the bottom-left expand toggle, and a markdown-source readout proving Markdown mode no longer backslash-escapes the user's own syntax (Stage 5a), all reloading live across Auto/Light/Dark."
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)
