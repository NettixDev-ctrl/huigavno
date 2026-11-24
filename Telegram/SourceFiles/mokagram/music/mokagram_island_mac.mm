// This file is part of mokaGram, a Telegram Desktop fork.
// SPDX-License-Identifier: GPL-3.0-only
#include "mokagram/music/mokagram_island_mac.h"

#include "ui/platform/ui_platform_utility.h"

#include <Cocoa/Cocoa.h>

namespace Mokagram::Music {
namespace {

struct DesktopGeometry {
	QRect area;
	QSize notch = QSize(0, 0);
	bool valid = false;
};

[[nodiscard]] const DesktopGeometry &ReadDesktopGeometry() {
	static auto cache = DesktopGeometry();
	[[maybe_unused]] static const auto observer = [[NSNotificationCenter defaultCenter]
		addObserverForName:NSApplicationDidChangeScreenParametersNotification
		object:nil
		queue:nil
		usingBlock:^(NSNotification *) {
			cache.valid = false;
		}];
	if (cache.valid) {
		return cache;
	}
	cache = DesktopGeometry();
	cache.valid = true;
	const auto screens = [NSScreen screens];
	const auto primary = [screens firstObject];
	if (!primary) {
		return cache;
	}
	auto selected = primary;
	for (NSScreen *screen in screens) {
		if (@available(macOS 12.0, *)) {
			if (screen.safeAreaInsets.top > 0.) {
				selected = screen;
				cache.notch = QSize(int(NSWidth(screen.frame)
					- NSWidth(screen.auxiliaryTopLeftArea)
					- NSWidth(screen.auxiliaryTopRightArea)),
					int(screen.safeAreaInsets.top));
				break;
			}
		}
	}
	const auto frame = selected.frame;
	// AppKit uses a bottom-left origin, Qt uses the primary screen's top-left.
	cache.area = QRect(
		int(NSMinX(frame)),
		int(NSMaxY(primary.frame) - NSMaxY(frame)),
		int(NSWidth(frame)),
		int(NSHeight(frame)));
	return cache;
}

} // namespace

void InitDesktopIsland(not_null<QWidget*> widget) {
	Ui::Platform::InitOnTopPanel(widget);
	const auto window = [reinterpret_cast<NSView*>(widget->winId()) window];
	[window setHasShadow:NO];
	[window setLevel:NSMainMenuWindowLevel + 3];
	[window setAnimationBehavior:NSWindowAnimationBehaviorNone];
	[window setMovable:NO];
}

void SetDesktopIslandMouseInput(not_null<QWidget*> widget, bool enabled) {
	const auto window = [reinterpret_cast<NSView*>(widget->winId()) window];
	if (window.ignoresMouseEvents == enabled) {
		[window setIgnoresMouseEvents:!enabled];
	}
}

QRect DesktopIslandArea() {
	return ReadDesktopGeometry().area;
}

QSize DesktopIslandNotchSize() {
	return ReadDesktopGeometry().notch;
}

} // namespace Mokagram::Music
