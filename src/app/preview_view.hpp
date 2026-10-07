#pragma once

#include <QGraphicsScene>
#include <QGraphicsView>

#include "core/document.hpp"

class QWheelEvent;
class QShowEvent;

namespace xm::app {

class PreviewView final : public QGraphicsView {
public:
    explicit PreviewView(QWidget* parent = nullptr);

    void setDocument(const core::GeometryDocument& document);
    void fitDocument();
    void resetView();

protected:
    void showEvent(QShowEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    QGraphicsScene scene_;
    bool hasContent_ = false;
};

}  // namespace xm::app
