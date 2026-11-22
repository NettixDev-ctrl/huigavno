# Punto plugin

## Assumptions

- Use the standard desktop US English and Russian keyboard layouts.
- Use the same-chat reply target that the plugin command API provides.
- Replace the command with a corrected reply so it also works on messages from other people.

## Acceptance criteria

- Sending `.punto` as a reply converts the target's text or caption. It sends the converted text as the reply to the same message.
- The layout with more letters in the text determines the direction. If neither layout has more letters, show a local notice and send nothing.
- Convert matching keys in both directions, including uppercase letters and punctuation. Keep spaces, line breaks, digits, and emoji.
- Keep supported formatting and its offsets in the corrected reply.
- Show a local notice if there is no reply, the target is unavailable, or the target has no text. The command text stays out of the chat.
