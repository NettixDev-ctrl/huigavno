# Live listening status

## Assumptions

- Viewing and sharing have separate settings. Both start off. Viewing sends user IDs from the open profile, direct chat, or visible chat list to the bot. The settings apply to this installation and explain what the bot receives.
- Share a track title and artist from the built-in MokaGram player. Include the public SoundCloud track ID when available. Include Telegram tracks only when forwarding is allowed. Omit audio, links, chat IDs, and playback position.
- Send listening-status requests to `@mokamusic_testbot` using Telegram inline queries. Use the existing bot destination for all other requests.
- Use a static accent ring on the profile, direct-chat, and visible chat-list avatars. Keep story rings and saved profile music available.

## Acceptance criteria

- The profile shows a track card above Channel with the track title and artist. The chat header shows the usual status and a ring around the avatar.
- The profile card shows the track cover when the player cache or SoundCloud has one.
- Activating the card or a ringed avatar opens the track in the player when its public SoundCloud ID is available. Other tracks open music search with their shared text.
- The track and ring disappear after a pause, stop, or opt-out reaches the viewer. The status also expires when the publisher loses its connection or closes the client.
- Switching chats never shows a previous user's track. Bot errors and invalid responses leave the normal profile and chat status usable.

## Bot contract

- `presence:v1:put:<writer>:<base64url>` sets a 35-second lease for the authenticated inline query sender. The writer is a random hexadecimal value of at most 16 digits. The payload encodes a UTF-8 track label of up to 150 bytes, without padding. The label starts with the title and includes the artist when available. An empty label is invalid.
- `presence:v1:clear:<writer>` removes the authenticated sender's lease when its writer matches. A clear from an earlier client must leave a newer client's lease in place.
- `presence:v1:get:<user_id>` returns a lease for a positive Telegram user ID. Return no result when the lease is absent or expired.
- `presence:v1:put:<writer>:<soundcloud_id>:<base64url>` adds a validated public SoundCloud ID to the same lease.
- `presence:v1:list:<user_id>,...` gets up to 12 visible peers in one request. Return one article per active lease with ID `presence-v1:<user_id>`. Its description contains the remaining milliseconds, followed by a colon and the public track ID when present.
- Return one article with ID `presence-v1`. For a read, put the label in `title` and the remaining lease in milliseconds in `description`. A successful write or clear only needs the ID.
- Set `cache_time` to `0` and `is_personal` to `true`. Rate limit by sender. Derive the publisher identity from the inline query sender. Keep only the current lease and remove expired records. Never log the track label.
