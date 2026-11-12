#include "app/BubbleStyle.hpp"

#include <QCoreApplication>
#include <QJSEngine>

#include <algorithm>
#include <cmath>

namespace core = screencam::core;

BubbleStyle *BubbleStyle::instance() {
    static BubbleStyle *style = new BubbleStyle(qApp);
    return style;
}

BubbleStyle *BubbleStyle::create(QQmlEngine *, QJSEngine *) {
    BubbleStyle *style = instance();
    QJSEngine::setObjectOwnership(style, QJSEngine::CppOwnership);
    return style;
}

BubbleStyle::BubbleStyle(QObject *parent)
    : QObject(parent) {
    m_store.beginGroup(QStringLiteral("bubble"));
    m_style.shape = static_cast<core::BubbleShape>(m_store.value("shape", static_cast<int>(m_style.shape)).toInt());
    m_style.size = m_store.value("size", m_style.size).toDouble();
    m_style.roundness = m_store.value("roundness", m_style.roundness).toDouble();
    m_style.mirrored = m_store.value("mirrored", m_style.mirrored).toBool();
    m_style.borderWidth = m_store.value("borderWidth", m_style.borderWidth).toDouble();
    m_style.borderColor = QColor(m_store.value("borderColor", m_style.borderColor.name(QColor::HexArgb)).toString());
    m_style.shadow = m_store.value("shadow", m_style.shadow).toBool();
    m_style.center.x = m_store.value("centerX", m_style.center.x).toDouble();
    m_style.center.y = m_store.value("centerY", m_style.center.y).toDouble();
}

void BubbleStyle::commit() {
    m_store.setValue("shape", static_cast<int>(m_style.shape));
    m_store.setValue("size", m_style.size);
    m_store.setValue("roundness", m_style.roundness);
    m_store.setValue("mirrored", m_style.mirrored);
    m_store.setValue("borderWidth", m_style.borderWidth);
    m_store.setValue("borderColor", m_style.borderColor.name(QColor::HexArgb));
    m_store.setValue("shadow", m_style.shadow);
    m_store.setValue("centerX", m_style.center.x);
    m_store.setValue("centerY", m_style.center.y);
    m_revision++;
    emit styleChanged();
}

void BubbleStyle::setShape(Shape shape) {
    const auto value = static_cast<core::BubbleShape>(shape);
    if (m_style.shape != value) {
        m_style.shape = value;
        commit();
    }
}

void BubbleStyle::setSize(double size) {
    const double value = std::clamp(size, 0.08, 0.6);
    if (std::abs(m_style.size - value) > 1e-6) {
        m_style.size = value;
        commit();
    }
}

void BubbleStyle::setRoundness(double roundness) {
    const double value = std::clamp(roundness, 0.0, 1.0);
    if (std::abs(m_style.roundness - value) > 1e-6) {
        m_style.roundness = value;
        commit();
    }
}

void BubbleStyle::setMirrored(bool mirrored) {
    if (m_style.mirrored != mirrored) {
        m_style.mirrored = mirrored;
        commit();
    }
}

void BubbleStyle::setBorderWidth(double width) {
    const double value = std::clamp(width, 0.0, 20.0);
    if (std::abs(m_style.borderWidth - value) > 1e-6) {
        m_style.borderWidth = value;
        commit();
    }
}

void BubbleStyle::setBorderColor(const QColor &color) {
    if (m_style.borderColor != color) {
        m_style.borderColor = color;
        commit();
    }
}

void BubbleStyle::setShadow(bool shadow) {
    if (m_style.shadow != shadow) {
        m_style.shadow = shadow;
        commit();
    }
}

core::SizeD BubbleStyle::bubbleSizeOn(double canvasWidth, double canvasHeight) const {
    return core::bubbleSize(core::SizeD{canvasWidth, canvasHeight}, m_style.shape, m_style.size);
}

QRectF BubbleStyle::rectFor(double canvasWidth, double canvasHeight) const {
    const core::RectD r =
        core::bubbleRect(core::SizeD{canvasWidth, canvasHeight}, m_style.shape, m_style.size, m_style.center);
    return {r.x, r.y, r.width, r.height};
}

double BubbleStyle::radiusFor(double bubbleWidth, double bubbleHeight) const {
    return core::bubbleCornerRadius(m_style.shape, core::SizeD{bubbleWidth, bubbleHeight}, m_style.roundness);
}

double BubbleStyle::borderFor(double canvasWidth, double canvasHeight) const {
    return m_style.borderPixels(canvasWidth, canvasHeight);
}

void BubbleStyle::moveCenter(double normalizedX, double normalizedY, double canvasWidth, double canvasHeight) {
    const core::SizeD canvas{canvasWidth, canvasHeight};
    m_style.center =
        core::clampNormalizedCenter(canvas, bubbleSizeOn(canvasWidth, canvasHeight), core::PointD{normalizedX, normalizedY});
    commit();
}

void BubbleStyle::snap(double canvasWidth, double canvasHeight, double threshold) {
    const core::SizeD canvas{canvasWidth, canvasHeight};
    m_style.center =
        core::snapNormalizedCenter(canvas, bubbleSizeOn(canvasWidth, canvasHeight), m_style.center, threshold);
    commit();
}

void BubbleStyle::placeInCorner(Corner corner, double canvasWidth, double canvasHeight) {
    const bool left = (corner == Corner::TopLeft || corner == Corner::BottomLeft);
    const bool top = (corner == Corner::TopLeft || corner == Corner::TopRight);
    // Aim past the edge; clamping puts the bubble exactly one margin away from it.
    moveCenter(left ? 0.0 : 1.0, top ? 0.0 : 1.0, canvasWidth, canvasHeight);
}
