#pragma once

#include "core/BubbleGeometry.hpp"
#include "media/BubbleStyleSnapshot.hpp"

#include <QColor>
#include <QObject>
#include <QRectF>
#include <QSettings>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;

/// How the camera bubble looks and where it sits (QML singleton `BubbleStyle`). The position
/// is a normalized centre, so it means the same in the preview and in a 4K recording.
class BubbleStyle : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(Shape shape READ shape WRITE setShape NOTIFY styleChanged)
    Q_PROPERTY(double size READ size WRITE setSize NOTIFY styleChanged)
    Q_PROPERTY(double roundness READ roundness WRITE setRoundness NOTIFY styleChanged)
    Q_PROPERTY(bool mirrored READ mirrored WRITE setMirrored NOTIFY styleChanged)
    Q_PROPERTY(double borderWidth READ borderWidth WRITE setBorderWidth NOTIFY styleChanged)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY styleChanged)
    Q_PROPERTY(bool shadow READ shadow WRITE setShadow NOTIFY styleChanged)
    Q_PROPERTY(double centerX READ centerX NOTIFY styleChanged)
    Q_PROPERTY(double centerY READ centerY NOTIFY styleChanged)
    // Bumped on every change: QML bindings that call rectFor() list it to re-evaluate.
    Q_PROPERTY(int revision READ revision NOTIFY styleChanged)

public:
    enum class Shape {
        Circle = static_cast<int>(screencam::core::BubbleShape::Circle),
        Squircle = static_cast<int>(screencam::core::BubbleShape::Squircle),
        Portrait = static_cast<int>(screencam::core::BubbleShape::Portrait),
        Landscape = static_cast<int>(screencam::core::BubbleShape::Landscape),
    };
    Q_ENUM(Shape)

    enum class Corner { TopLeft, TopRight, BottomLeft, BottomRight };
    Q_ENUM(Corner)

    static BubbleStyle *instance();
    static BubbleStyle *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    /// `persistent`: kept in QSettings (the default look). The editor's isn't.
    explicit BubbleStyle(QObject *parent = nullptr, bool persistent = false);

    Shape shape() const { return static_cast<Shape>(m_style.shape); }
    void setShape(Shape shape);
    double size() const { return m_style.size; }
    void setSize(double size);
    double roundness() const { return m_style.roundness; }
    void setRoundness(double roundness);
    bool mirrored() const { return m_style.mirrored; }
    void setMirrored(bool mirrored);
    double borderWidth() const { return m_style.borderWidth; }
    void setBorderWidth(double width);
    QColor borderColor() const { return m_style.borderColor; }
    void setBorderColor(const QColor &color);
    bool shadow() const { return m_style.shadow; }
    void setShadow(bool shadow);
    double centerX() const { return m_style.center.x; }
    double centerY() const { return m_style.center.y; }
    int revision() const { return m_revision; }

    BubbleStyleSnapshot snapshot() const { return m_style; }
    void setSnapshot(const BubbleStyleSnapshot &style);

    Q_INVOKABLE QRectF rectFor(double canvasWidth, double canvasHeight) const;
    Q_INVOKABLE double radiusFor(double bubbleWidth, double bubbleHeight) const;
    Q_INVOKABLE double borderFor(double canvasWidth, double canvasHeight) const;
    Q_INVOKABLE void moveCenter(double normalizedX, double normalizedY, double canvasWidth, double canvasHeight);
    /// When a drag ends.
    Q_INVOKABLE void snap(double canvasWidth, double canvasHeight, double threshold = 0.06);
    Q_INVOKABLE void placeInCorner(Corner corner, double canvasWidth, double canvasHeight);

signals:
    void styleChanged();

private:
    void commit();  // save + notify
    screencam::core::SizeD bubbleSizeOn(double canvasWidth, double canvasHeight) const;

    bool m_persistent = false;
    QSettings m_store;
    BubbleStyleSnapshot m_style;
    int m_revision = 0;
};
