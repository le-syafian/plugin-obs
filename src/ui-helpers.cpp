#include "ui-helpers.hpp"

#include <QDateTime>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace {

QPixmap BlankPixmap(int size)
{
	const qreal dpr = qGuiApp ? qGuiApp->devicePixelRatio() : 1.0;
	QPixmap pixmap(QSize(size, size) * dpr);
	pixmap.setDevicePixelRatio(dpr);
	pixmap.fill(Qt::transparent);
	return pixmap;
}

} // namespace

bool TallyBlinkOn()
{
	return (QDateTime::currentMSecsSinceEpoch() / kTallyBlinkMs) % 2 == 0;
}

void DrawOnAirDot(QPainter &painter, const QPointF &center, qreal radius, const QColor &color, bool lit)
{
	painter.save();
	painter.setRenderHint(QPainter::Antialiasing, true);
	painter.setPen(Qt::NoPen);

	QColor dot = color;
	if (lit) {
		QColor halo = color;
		halo.setAlphaF(0.28);
		painter.setBrush(halo);
		painter.drawEllipse(center, radius + 2.5, radius + 2.5);
	} else {
		dot.setAlphaF(0.3);
	}

	painter.setBrush(dot);
	painter.drawEllipse(center, radius, radius);
	painter.restore();
}

void DrawPreviewEye(QPainter &painter, const QRectF &rect, const QColor &color)
{
	painter.save();
	painter.setRenderHint(QPainter::Antialiasing, true);

	const QPointF left(rect.left(), rect.center().y());
	const QPointF right(rect.right(), rect.center().y());
	QPainterPath outline;
	outline.moveTo(left);
	outline.quadTo(QPointF(rect.center().x(), rect.top() - rect.height() * 0.35), right);
	outline.quadTo(QPointF(rect.center().x(), rect.bottom() + rect.height() * 0.35), left);

	painter.setPen(QPen(color, 1.4));
	painter.setBrush(Qt::NoBrush);
	painter.drawPath(outline);

	painter.setPen(Qt::NoPen);
	painter.setBrush(color);
	const qreal pupil = rect.height() * 0.3;
	painter.drawEllipse(rect.center(), pupil, pupil);

	painter.restore();
}

QColor ContrastingText(const QColor &background)
{
	const double luma = 0.299 * background.redF() + 0.587 * background.greenF() + 0.114 * background.blueF();
	return luma > 0.6 ? QColor(24, 24, 24) : QColor(255, 255, 255);
}

QIcon ColorSwatchIcon(const QColor &color)
{
	QPixmap pixmap = BlankPixmap(16);
	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing, true);
	painter.setPen(QPen(color.darker(150), 1));
	painter.setBrush(color);
	painter.drawRoundedRect(QRectF(1.5, 1.5, 13, 13), 3, 3);
	return QIcon(pixmap);
}

QIcon MakeMediaIcon(MediaIcon icon, const QColor &color)
{
	QPixmap pixmap = BlankPixmap(16);
	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing, true);
	painter.setPen(Qt::NoPen);
	painter.setBrush(color);

	switch (icon) {
	case MediaIcon::Play: {
		QPainterPath triangle;
		triangle.moveTo(4.5, 2.5);
		triangle.lineTo(13.5, 8);
		triangle.lineTo(4.5, 13.5);
		triangle.closeSubpath();
		painter.drawPath(triangle);
		break;
	}
	case MediaIcon::Pause:
		painter.drawRoundedRect(QRectF(3.5, 3, 3.2, 10), 1, 1);
		painter.drawRoundedRect(QRectF(9.3, 3, 3.2, 10), 1, 1);
		break;
	case MediaIcon::Restart: {
		/* |◀ : back to the start */
		painter.drawRoundedRect(QRectF(3, 3, 2.2, 10), 0.8, 0.8);
		QPainterPath triangle;
		triangle.moveTo(13.5, 2.5);
		triangle.lineTo(5.8, 8);
		triangle.lineTo(13.5, 13.5);
		triangle.closeSubpath();
		painter.drawPath(triangle);
		break;
	}
	case MediaIcon::Stop:
		painter.drawRoundedRect(QRectF(3.5, 3.5, 9, 9), 1.5, 1.5);
		break;
	}

	return QIcon(pixmap);
}
