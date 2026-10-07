#include "app/preview_view.hpp"

#include <QBrush>
#include <QColor>
#include <QGraphicsItem>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QShowEvent>
#include <QVariant>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <string>
#include <type_traits>
#include <variant>

#include "core/entity.hpp"

namespace xm::app {
namespace {

QColor colorForAci(int index)
{
    switch (index) {
    case 1: return QColor(239, 83, 80);
    case 2: return QColor(255, 202, 40);
    case 3: return QColor(102, 187, 106);
    case 4: return QColor(38, 198, 218);
    case 5: return QColor(66, 165, 245);
    case 6: return QColor(171, 71, 188);
    case 7: return QColor(224, 224, 224);
    default:
        return QColor::fromHsv((std::clamp(index, 1, 255) * 47) % 360,
                               150, 235);
    }
}

bool finite(const core::Point2d& point) noexcept
{
    return std::isfinite(point.x) && std::isfinite(point.y);
}

}  // namespace

PreviewView::PreviewView(QWidget* parent)
    : QGraphicsView(parent)
{
    setScene(&scene_);
    setBackgroundBrush(QColor(25, 29, 35));
    setRenderHint(QPainter::Antialiasing, true);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setFrameShape(QFrame::NoFrame);
}

void PreviewView::setDocument(const core::GeometryDocument& document)
{
    scene_.clear();

    std::map<std::string, QColor> colors;
    std::map<std::string, bool> visibility;
    colors.emplace("0", colorForAci(7));
    visibility.emplace("0", true);
    for (const core::Layer& layer : document.layers()) {
        colors[layer.name] = colorForAci(layer.colorIndex);
        visibility[layer.name] = layer.visible;
    }

    for (const core::GeometryEntity& entity : document.entities()) {
        const auto visible = visibility.find(entity.layer);
        if (visible != visibility.end() && !visible->second) {
            continue;
        }
        const auto color = colors.find(entity.layer);
        QPen pen(color == colors.end() ? colorForAci(7) : color->second);
        pen.setWidthF(0.0);
        pen.setCosmetic(true);

        QGraphicsItem* item = std::visit(
            [this, &pen](const auto& shape) -> QGraphicsItem* {
                using T = std::decay_t<decltype(shape)>;
                if constexpr (std::is_same_v<T, core::Line>) {
                    if (!finite(shape.from) || !finite(shape.to)) {
                        return nullptr;
                    }
                    return scene_.addLine(shape.from.x, shape.from.y,
                                          shape.to.x, shape.to.y, pen);
                } else if constexpr (std::is_same_v<T, core::Circle>) {
                    if (!finite(shape.center) || !std::isfinite(shape.radius)
                        || shape.radius <= 0.0) {
                        return nullptr;
                    }
                    return scene_.addEllipse(shape.center.x - shape.radius,
                                             shape.center.y - shape.radius,
                                             shape.radius * 2.0,
                                             shape.radius * 2.0,
                                             pen, Qt::NoBrush);
                } else if constexpr (std::is_same_v<T, core::Arc>) {
                    const double sweep = core::arcSweepDegrees(shape);
                    if (!finite(shape.center) || !std::isfinite(shape.radius)
                        || shape.radius <= 0.0 || !std::isfinite(sweep)
                        || sweep == 0.0) {
                        return nullptr;
                    }
                    const QRectF bounds(shape.center.x - shape.radius,
                                        shape.center.y - shape.radius,
                                        shape.radius * 2.0,
                                        shape.radius * 2.0);
                    QPainterPath path;
                    // Qt arc angles assume a screen-style Y axis. The view is
                    // flipped to world Y-up, so negate both angles here.
                    const double start = -core::normalizeDegrees(shape.startDegrees);
                    path.arcMoveTo(bounds, start);
                    path.arcTo(bounds, start, -sweep);
                    return scene_.addPath(path, pen, Qt::NoBrush);
                } else {
                    if (shape.points.empty()) {
                        return nullptr;
                    }
                    if (!std::all_of(shape.points.begin(), shape.points.end(), finite)) {
                        return nullptr;
                    }
                    QPainterPath path;
                    path.moveTo(shape.points.front().x, shape.points.front().y);
                    for (std::size_t index = 1; index < shape.points.size(); ++index) {
                        path.lineTo(shape.points[index].x, shape.points[index].y);
                    }
                    if (shape.closed) {
                        path.closeSubpath();
                    }
                    return scene_.addPath(path, pen, Qt::NoBrush);
                }
            },
            entity.shape);
        if (item != nullptr) {
            item->setData(0, QVariant::fromValue<qulonglong>(entity.id));
        }
    }

    hasContent_ = !scene_.items().empty();
    if (hasContent_) {
        QRectF bounds = scene_.itemsBoundingRect();
        bounds.adjust(-20.0, -20.0, 20.0, 20.0);
        scene_.setSceneRect(bounds);
        fitDocument();
    } else {
        scene_.setSceneRect(QRectF(-50.0, -50.0, 100.0, 100.0));
        resetView();
    }
}

void PreviewView::fitDocument()
{
    resetTransform();
    scale(1.0, -1.0);
    if (hasContent_) {
        fitInView(scene_.sceneRect(), Qt::KeepAspectRatio);
    } else {
        centerOn(0.0, 0.0);
    }
}

void PreviewView::resetView()
{
    resetTransform();
    scale(1.0, -1.0);
    centerOn(scene_.sceneRect().center());
}

void PreviewView::showEvent(QShowEvent* event)
{
    QGraphicsView::showEvent(event);
    fitDocument();
}

void PreviewView::wheelEvent(QWheelEvent* event)
{
    const int delta = event->angleDelta().y();
    if (delta == 0) {
        QGraphicsView::wheelEvent(event);
        return;
    }
    const double factor = delta > 0 ? 1.15 : 1.0 / 1.15;
    const double nextScale = std::abs(transform().m11()) * factor;
    if (nextScale >= 0.01 && nextScale <= 1000.0) {
        scale(factor, factor);
    }
    event->accept();
}

}  // namespace xm::app
