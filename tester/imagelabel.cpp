#include "imagelabel.h"

ImageLabel::ImageLabel(QWidget *parent, Qt::WindowFlags)
{
    setMouseTracking(true);
}

void ImageLabel::mouseMoveEvent(QMouseEvent *ev)
{
    emit mouseMoved(ev->pos());
}

void ImageLabel::mouseReleaseEvent(QMouseEvent *ev)
{
    emit mouseClicked(ev->pos());
}
