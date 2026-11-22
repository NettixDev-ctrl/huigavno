// This file is part of mokaGram, https://github.com/ecronxy/mokagram
// (c) 2026 @ecronx (https://t.me/ecronx). GPLv3 with the attribution term
// of MOKAGRAM_NOTICE.md (section 7(b)): keep this notice when copying.
#pragma once

#include <QtCore/QString>

namespace Main {
class Session;
} // namespace Main

namespace Mokagram::TyankaHikka {

void Start();
void Finish();
[[nodiscard]] bool IsArtist(const QString &name);
void Unlock();
void OnOwnMessage(not_null<Main::Session*> session, const QString &text);

} // namespace Mokagram::TyankaHikka
