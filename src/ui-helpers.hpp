#pragma once

#include <QColor>
#include <QIcon>
#include <QPointF>
#include <QRectF>

class QPainter;

/* ON AIR dots blink with this half period, in sync everywhere */
constexpr int kTallyBlinkMs = 500;

bool TallyBlinkOn();

/* Blinking ON AIR dot: lit = full color with a halo, otherwise faded.
 * Takes 2 * (radius + 2.5) px including the halo. */
void DrawOnAirDot(QPainter &painter, const QPointF &center, qreal radius, const QColor &color, bool lit);

/* PREVIEW symbol: an eye */
void DrawPreviewEye(QPainter &painter, const QRectF &rect, const QColor &color);

/* Black or white, whichever reads better on the given background */
QColor ContrastingText(const QColor &background);

/* Small rounded color square for menus and buttons */
QIcon ColorSwatchIcon(const QColor &color);

enum class MediaIcon {
	Play,
	Pause,
	Restart,
	Stop,
};

QIcon MakeMediaIcon(MediaIcon icon, const QColor &color);
