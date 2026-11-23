/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/effects/ripple_animation.h"
#include "ui/painter.h"
#include "ui/widgets/buttons.h"

namespace Info::Profile {

// Mokagram: profile cards already use windowBgOver as their surface.
class CardButton final : public Ui::SettingsButton {
public:
	using Ui::SettingsButton::SettingsButton;

private:
	void paintEvent(QPaintEvent *) override {
		auto p = Painter(this);
		const auto over = (isOver() || isDown()) && !isDisabled();
		if (over) {
			auto hq = PainterHighQualityEnabler(p);
			auto background = st::windowFg->c;
			background.setAlphaF(0.08);
			p.setPen(Qt::NoPen);
			p.setBrush(background);
			const auto radius = std::min(width(), height()) / 2.;
			p.drawRoundedRect(QRectF(rect()), radius, radius);
		}
		paintRipple(p, 0, 0);
		paintText(p, over, width());
		if (!maybeToggleRect().isEmpty()) {
			paintToggle(p, width());
		}
	}

	QImage prepareRippleMask() const override {
		return Ui::RippleAnimation::RoundRectMask(
			size(),
			std::min(width(), height()) / 2);
	}
};

} // namespace Info::Profile
