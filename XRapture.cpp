#include <QApplication>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPathItem>
#include <QMouseEvent>
#include <QGraphicsBlurEffect>
#include <QColorDialog>
#include <QScrollBar>
#include <QThread>
#include <QMenu>
#include <QTimer>
#include <QEventLoop>
#include <iostream>
#include <slop.hpp>
#include <QDebug>
#include <QClipboard>
#include <QMimeData>
#include <QPair>
#include <QFileDialog>
#include <QUndoCommand>
#include <complex>

#include "TextInputDialog.hpp"
#include "XRapture.hpp"

class AddItemCommand : public QUndoCommand
{
public:
  AddItemCommand(QGraphicsScene* scene, QGraphicsItem* item, QUndoCommand *parent = 0):
    QUndoCommand(parent), scene_(scene), item_(item) {
  }

  void undo() {
    scene_ -> removeItem(item_);
  }

  void redo() {
    scene_ -> addItem(item_);
  }

  ~AddItemCommand() {
    delete item_;
  }

private:
  QGraphicsScene* scene_;
  QGraphicsItem* item_;
};

class TransformCommand : public QUndoCommand
{
public:
  TransformCommand(XRapture* view, QTransform* trans, QTransform oldTrans, QTransform newTrans, QUndoCommand *parent = 0):
    QUndoCommand(parent), view_(view), trans_(trans), oldTrans_(oldTrans), newTrans_(newTrans) {
  }

  void undo() {
    *trans_ = oldTrans_;
    view_ -> calcTransform();
  }

  void redo() {
    *trans_ = newTrans_;
    view_ -> calcTransform();
  }

  ~TransformCommand() {
  }

private:
  XRapture* view_;
  QTransform* trans_;
  QTransform oldTrans_;
  QTransform newTrans_;
};

QImage XRapture::applyEffect(const QPixmap& src, QGraphicsEffect *effect, const QRect& rect) const
{
  QGraphicsScene scene;
  QGraphicsPixmapItem item;

  item.setPixmap(src);
  item.setGraphicsEffect(effect);
  scene.addItem(&item);

  QImage ret(QSize(rect.width(), rect.height()), QImage::Format_ARGB32);
  QPainter painter(&ret);

  scene.render(&painter, QRect(), rect);

  return ret;
}

QPixmap XRapture::CreateColorPixmap(const QColor& color) const
{
  QPixmap pixmap(100, 100);
  pixmap.fill(color);

  QPainter paint(&pixmap);
  paint.setPen(QColor(100, 100, 100, 255));
  paint.drawRect(0, 0, pixmap.height() - 1, pixmap.width() - 1);

  return pixmap;
}

QPainterPath XRapture::CreateArrow(const QPointF& p1, const QPointF& p2, int width) const
{
  QPainterPath path;
  QLineF base(p1, p2);

  path.moveTo(p1);
  path.lineTo(p2);

  width = width + 1;

  QLineF l1(0, 0, width * 2, width * 2);
  l1.setAngle(base.angle() - 140);
  l1.translate(p2);
  path.lineTo(l1.x2(), l1.y2());

  QLineF l2(0, 0, width * 2, width * 2);
  l2.setAngle(base.angle() + 140);
  l2.translate(p2);
  path.moveTo(p2);
  path.lineTo(l2.x2(), l2.y2());

  return path;
}

QPainterPath XRapture::CreateArrow2(const QPointF& p1, const QPointF& p2, int width) const
{
  struct Local {
    static QLineF makeLine(QPointF p, qreal angle, int w) {
      QLineF base2(0, 0, w, w);
      base2.setAngle(angle + 90);
      base2.translate(p);

      QLineF base3(0, 0, w, w);
      base3.setAngle(angle - 90);
      base3.translate(p);

      return QLineF(base2.p2(), base3.p2());
    }
  };

  QVector<QPointF> points;
  QPainterPath path;
  QLineF line(p1, p2);

  width /= 2;

  QLineF lineOffset = QLineF(0, 0, (width * 2 + 2) + width, (width * 2 + 2) + width);
  lineOffset.setAngle(line.angle() - 180);
  lineOffset.translate(p2);

  auto lineBase1 = Local::makeLine(line.p1(), line.angle(), width);
  points.push_back(lineBase1.p1());
  points.push_back(lineBase1.p2());

  auto lineBase2 = Local::makeLine(lineOffset.p2(), line.angle(), width);
  QLineF arrowBase = Local::makeLine(lineOffset.p2(), line.angle(), (width * 2 + 2));

  points.push_back(lineBase2.p2());
  points.push_back(arrowBase.p2());

  points.push_back(p2);

  points.push_back(arrowBase.p1());
  points.push_back(lineBase2.p1());

  points.push_back(lineBase1.p1());

  path.addPolygon(QPolygonF(points));

  return path;
}

XRapture::XRapture(QGraphicsScene* scene)
  : QGraphicsView(scene), titleBar_(false), textMode_(false), zoomScale_(100),
    undoStack_(new QUndoStack), color_(Qt::red), lineWidth_(4), highlighter_(false),
    pixmap_(0), preDrawItem_(0), oldButton_(Qt::NoButton), oldX_(0), oldY_(0),
    drawMode_(DrawMode::FREE_LINE)
{
  this -> setObjectName("XRapture");

  if(titleBar_) {
    this -> setWindowFlags(Qt::WindowStaysOnTopHint |
                           Qt::WindowMinimizeButtonHint |
                           Qt::WindowCloseButtonHint);
    this -> setStyleSheet("#XRapture {background: transparent; border: none;}");
  }
  else {
    this -> setWindowFlags(Qt::WindowStaysOnTopHint |
                           Qt::FramelessWindowHint);
    this -> setStyleSheet("#XRapture {background-color: white;}");
  }

  this -> setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  this -> setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  this -> setRenderHints( QPainter::Antialiasing | QPainter::HighQualityAntialiasing );
}

void XRapture::changeWindowGeometry(int x, int y, int w, int h)
{
  if(titleBar_) {
    this -> setMaximumSize(w, h);
    this -> setGeometry(x, y, w, h);
  }
  else {
    this -> setMaximumSize(w + 2, h + 2);
    this -> setGeometry(x, y, w + 2, h + 2);
  }
}

void XRapture::screenCapture(int x, int y, int w, int h)
{
  QScreen* screen = QGuiApplication::primaryScreen();

  if(w % 2 != 0) ++w;
  if(h % 2 != 0) ++h;

  this -> setPixmap(screen -> grabWindow(0, x, y, w, h));
  this -> setStyleSheet("#XRapture {background: transparent; border: none;}");
  this -> setGeometry(x, y, 1, 1);
  this -> show();
  this -> mSleep(300);

  if(titleBar_) {
    this -> changeWindowGeometry(x, y, w, h);
  }
  else {
    this -> changeWindowGeometry(x - 1, y - 1, w, h);
    this -> setStyleSheet("#XRapture {background-color: white;}");
  }
}

void XRapture::setPixmap(QPixmap pixmap)
{
  int w = pixmap.width();
  int h = pixmap.height();

  undoStack_ -> clear();
  scale_.reset();
  mirror_.reset();
  rotation_.reset();
  zoomScale_ = 100;

  setTransform(scale_);
  this -> scene() -> clear();

  this -> scene() -> setSceneRect(0, 0, w, h);
  setSceneRect(0, 0, w, h);

  pixmap_ = this -> scene() -> addPixmap(pixmap);
  pixmap_ -> setTransformationMode(Qt::SmoothTransformation);
}

void XRapture::createEditSubMenu(QMenu* menu)
{
  QAction* action;
  auto editMenu = menu -> addMenu("&Edit");

  action = editMenu -> addAction("&Copy");
  action -> setShortcut(QKeySequence(Qt::CTRL + Qt::Key_C));

#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
  action -> setShortcutVisibleInContextMenu(true);
#endif

  connect(action, &QAction::triggered, this, &XRapture::copyAction);

  action = editMenu -> addAction("&Paste");
  action -> setShortcut(QKeySequence(Qt::CTRL + Qt::Key_V));

#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
  action -> setShortcutVisibleInContextMenu(true);
#endif

  connect(action, &QAction::triggered, this, &XRapture::pasteAction);
  const QClipboard *clipboard = QApplication::clipboard();
  const QMimeData *mimeData = clipboard -> mimeData();

  if(!mimeData -> hasImage()) action -> setDisabled(true);

  editMenu -> addSeparator();
  action = editMenu -> addAction("&Undo");
  action -> setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Z));

#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
  action -> setShortcutVisibleInContextMenu(true);
#endif

  connect(action, &QAction::triggered, this, &XRapture::undoAction);
  if(!undoStack_ -> canUndo()) action -> setDisabled(true);
}

void XRapture::createColorSubMenu(QMenu* menu)
{
  QAction* action;

  auto colorMenu = menu -> addMenu("&Color");
  colorMenu -> setIcon(QIcon(CreateColorPixmap(color_)));

  auto colors =
    {
     qMakePair(QString("&Black"),  Qt::black),
     qMakePair(QString("&Red"),    Qt::red),
     qMakePair(QString("&Green"),  Qt::green),
     qMakePair(QString("&Blue"),   Qt::blue),
     qMakePair(QString("&Yellow"), Qt::yellow),
     qMakePair(QString("&White"),  Qt::white),
    };

  for(auto color: colors) {
    action = colorMenu -> addAction(color.first);
    action -> setIcon(QIcon(CreateColorPixmap(color.second)));
    connect(action, &QAction::triggered,
            [=] {
              color_ = color.second;

              if(highlighter_) color_.setAlpha(128);
            }
            );
  }
  action = colorMenu -> addAction("&Other...");
  connect(action, &QAction::triggered,
          [=] {
            auto color = QColorDialog::getColor(color_, this);
            if(color.isValid()) color_ = color;

            if(highlighter_) color_.setAlpha(128);
          });
}

void XRapture::createLineWidthSubMenu(QMenu* menu)
{
  QAction* action;
  auto lineWidthSubMenu = menu -> addMenu("Line &Width");

  int lineWidths[] = {1, 2, 4, 8, 16, 24, 32};
  for(auto lineWidth: lineWidths) {
    action = lineWidthSubMenu -> addAction(QString::number(lineWidth) + "px");
    action -> setCheckable(true);
    if(lineWidth_ == lineWidth) action -> setChecked(true);

    connect(action, &QAction::triggered,
            [=] { lineWidth_ = lineWidth; }
            );
  }
}

void XRapture::createDrawSubMenu(QMenu* menu, QContextMenuEvent *event)
{
  QAction* action;
  auto draSubMenu = menu -> addMenu("&Draw");

  action = draSubMenu -> addAction("&Text");
  connect(action, &QAction::triggered,
          [=] {
            TextInputDialog dialog(color_, this);

            if(dialog.exec() == QDialog::Accepted) {
              textMode_ = true;
              QGraphicsSimpleTextItem* textItem = this -> scene() -> addSimpleText(dialog.getText(), dialog.getFont());

              QColor color = dialog.getColor();
              if(highlighter_) color.setAlpha(128);
              else color.setAlpha(255);

              QBrush brush(color);
              if(dialog.getOutline()) {
                auto pen = QPen(dialog.getOutlineColor());

                textItem -> setPen(pen);
              }

              textItem -> setBrush(brush);
              textItem -> setPos(mapToScene(event -> pos()));

              preDrawItem_ = textItem;
              preDrawItem_ -> setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
              preDrawItem_ -> setSelected(true);
            }
          }
          );

  auto drawModeMenus =
    {
     qMakePair(QString("Free L&ine"), DrawMode::FREE_LINE),
     qMakePair(QString("&Line"), DrawMode::LINE),
     qMakePair(QString("Arrow&1"), DrawMode::ARROW1),
     qMakePair(QString("Arrow&2"), DrawMode::ARROW2),
     qMakePair(QString("&Rect"), DrawMode::RECT),
     qMakePair(QString("&Fill Rect"), DrawMode::FILL_RECT),
     qMakePair(QString("&Blur Rect"), DrawMode::BLUR_RECT),
    };

  for(auto drawMode: drawModeMenus) {
    action = draSubMenu -> addAction(drawMode.first);
    action -> setCheckable(true);
    if(drawMode_ == drawMode.second) action -> setChecked(true);

    connect(action, &QAction::triggered,
            [=] { drawMode_ = drawMode.second; }
            );
  }
}

void XRapture::createTransformSubMenu(QMenu* menu)
{
  QAction* action;
  auto transSubMenu = menu -> addMenu("&Transform");

  action = transSubMenu -> addAction("Rotation &R");
  connect(action, &QAction::triggered,
          [=] { this -> rotationAction(90); }
          );
  action = transSubMenu -> addAction("Rotation &L");
  connect(action, &QAction::triggered,
          [=] { this -> rotationAction(-90); }
          );
  action = transSubMenu -> addAction("Mirror &H");
  connect(action, &QAction::triggered,
          [=] { this -> mirrorAction(true, false); }
          );
  action = transSubMenu -> addAction("Mirror &V");
  connect(action, &QAction::triggered,
          [=] { this -> mirrorAction(false, true); }
          );
}

void XRapture::createOpacitySubMenu(QMenu* menu)
{
  auto opacitySubMenu = menu -> addMenu("&Opacity");
  auto opacityGroup = new QActionGroup(this);
  auto winOpacity = windowOpacity();
  const char* opacityLabels[] = {"25", "50", "75", "100"};
  for(auto label: opacityLabels) {
    qreal opacity = atoi(label) / 100.0;
    auto action = new QAction(label, this);
    action -> setCheckable(true);

    connect(action, &QAction::triggered,
            [=] {
              this -> setWindowOpacity(opacity);
            }
            );
    opacityGroup -> addAction(action);

    if(winOpacity - 0.05 <= opacity && opacity <= winOpacity + 0.05)
      action -> setChecked(true);

    opacitySubMenu -> addAction(action);
  }
}

void XRapture::createZoomSubMenu(QMenu* menu)
{
  auto zoomSubMenu = menu -> addMenu("&Zoom");
  auto zoomGroup = new QActionGroup(this);
  const char* zoomLabels[] = {"50", "75", "100", "150", "200", "500"};
  for(auto label: zoomLabels) {
    int zoom = atoi(label);
    auto action = new QAction(label, this);
    action -> setCheckable(true);

#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    action -> setShortcutVisibleInContextMenu(true);
#endif

    if(zoom == 100) {
      action -> setShortcut(QKeySequence(Qt::Key_1));
    }
    if(zoom == 200) {
      action -> setShortcut(QKeySequence(Qt::Key_2));
    }
    if(zoom == 500) {
      action -> setShortcut(QKeySequence(Qt::Key_5));
    }

    connect(action, &QAction::triggered,
            [=] {
              zoomScale_ = zoom;
              this -> zoomAction(zoom / 100.0);
            }
            );
    zoomGroup -> addAction(action);
    if(zoomScale_ == zoom)
      action -> setChecked(true);

    zoomSubMenu -> addAction(action);
  }
}

void XRapture::contextMenuEvent(QContextMenuEvent *event)
{
  QMenu menu(this);
  QAction* action;

  auto fileSubMenu = menu.addMenu("&File");
  action = fileSubMenu -> addAction("&Open");
  action -> setShortcut(QKeySequence(Qt::CTRL + Qt::Key_O));

#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
  action -> setShortcutVisibleInContextMenu(true);
#endif

  connect(action, &QAction::triggered,
          [=] { openAction(); }
          );
  action = fileSubMenu -> addAction("&Save");
  action -> setShortcut(QKeySequence(Qt::CTRL + Qt::Key_S));

#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
  action -> setShortcutVisibleInContextMenu(true);
#endif

  connect(action, &QAction::triggered,
          [=] { saveAction(); }
          );
  this -> createEditSubMenu(&menu);

  menu.addSeparator();
  this -> createColorSubMenu(&menu);
  this -> createLineWidthSubMenu(&menu);
  action = menu.addAction("&Highlighter");
  action -> setCheckable(true);
  if(highlighter_) action -> setChecked(true);
  connect(action, &QAction::triggered,
          [=](bool set) {
            highlighter_ = set;
            if(highlighter_) color_.setAlpha(128);
            else color_.setAlpha(255);
          }
          );
  menu.addSeparator();

  this -> createDrawSubMenu(&menu, event);
  this -> createTransformSubMenu(&menu);
  menu.addSeparator();

  this -> createOpacitySubMenu(&menu);
  this -> createZoomSubMenu(&menu);
  action = menu.addAction("&ReCapture");
  connect(action, &QAction::triggered,
          [=] { reCaptureAction(); }
          );
  action = menu.addAction("&Quit");
  connect(action, &QAction::triggered, this, &XRapture::quitAction);

  menu.exec(event -> globalPos());
}

void XRapture::keyPressEvent(QKeyEvent *event)
{
  QWidget::keyPressEvent(event);

  if(event->isAutoRepeat()) return;

  if(event -> modifiers() == Qt::ControlModifier) {
    switch (event->key()) {
    case Qt::Key_O:
      this -> openAction();
      break;
    case Qt::Key_S:
      this -> saveAction();
      break;
    case Qt::Key_Z:
      this -> undoAction();
      break;
    case Qt::Key_C:
      this -> copyAction();
      break;
    case Qt::Key_V:
      this -> pasteAction();
      break;
    }

    return;
  }

  switch (event->key()) {
  case Qt::Key_1:
    zoomScale_ = 100;
    this -> zoomAction(zoomScale_ / 100.0);
    break;
  case Qt::Key_2:
    zoomScale_ = 200;
    this -> zoomAction(zoomScale_ / 100.0);
    break;
  case Qt::Key_5:
    zoomScale_ = 500;
    this -> zoomAction(zoomScale_ / 100.0);
    break;
  }

  return;
}

void XRapture::mousePressEvent(QMouseEvent* event)
{
  QGraphicsView::mousePressEvent(event);

  if(!textMode_) {
    oldButton_ = event -> button();
    oldMouseModifiers_ = event -> modifiers();
    oldX_ = event -> x();
    oldY_ = event -> y();

    if(preDrawItem_ != 0) {
      this -> scene() -> removeItem(preDrawItem_);
      auto addItemCommand = new AddItemCommand(this -> scene(), preDrawItem_);
      undoStack_ -> push(addItemCommand);
      preDrawItem_ = 0;
    }
  }
  else {
    auto items = this -> scene() -> selectedItems();
    if(items.size() == 0) {
      preDrawItem_ -> setFlags({});

      this -> scene() -> removeItem(preDrawItem_);
      auto addItemCommand = new AddItemCommand(this -> scene(), preDrawItem_);
      undoStack_ -> push(addItemCommand);

      preDrawItem_ = 0;
      textMode_ = false;
    }
  }
}

void XRapture::mouseReleaseEvent(QMouseEvent* event)
{
  if(!textMode_) {
    oldButton_ = Qt::NoButton;
    oldX_ = event -> x();
    oldY_ = event -> y();

    if(preDrawItem_ != 0) {
      this -> scene() -> removeItem(preDrawItem_);
      auto addItemCommand = new AddItemCommand(this -> scene(), preDrawItem_);
      undoStack_ -> push(addItemCommand);
      preDrawItem_ = 0;
    }
  }
  else {
    auto items = this -> scene() -> selectedItems();
    if(items.size() == 0) {
      preDrawItem_ -> setFlags({});

      this -> scene() -> removeItem(preDrawItem_);
      auto addItemCommand = new AddItemCommand(this -> scene(), preDrawItem_);
      undoStack_ -> push(addItemCommand);

      preDrawItem_ = 0;
      textMode_ = false;
    }
  }
  QGraphicsView::mouseReleaseEvent(event);
}

void XRapture::mouseMoveEvent(QMouseEvent* event)
{
  if(oldButton_ == Qt::MiddleButton) {
    int dx = oldX_ - event -> x();
    int dy = oldY_ - event -> y();

    int ox = this -> horizontalScrollBar() -> value();
    int oy = this -> verticalScrollBar() -> value();

    this -> horizontalScrollBar() -> setValue(ox + dx);
    this -> verticalScrollBar() -> setValue(oy + dy);

    oldX_ = event -> x();
    oldY_ = event -> y();
  }
  if(oldButton_ == Qt::LeftButton) {
    if(oldMouseModifiers_ == Qt::NoButton) {
      auto gpos = event -> globalPos();
      auto geometry = this -> geometry();

      this -> setGeometry(gpos.x() - oldX_, gpos.y() - oldY_,
                          geometry.width(), geometry.height());
    }

    if(oldMouseModifiers_ == Qt::ControlModifier) {
      auto point = mapToScene(event -> pos());
      auto oldPoint = mapToScene(QPoint(oldX_, oldY_));
      auto pen = QPen(color_);

      pen.setCapStyle(Qt::RoundCap);
      pen.setJoinStyle(Qt::RoundJoin);
      pen.setWidth(lineWidth_);

      switch (drawMode_) {
      case FREE_LINE:
        this -> drawFreeLine(oldPoint, point, pen);

        oldX_ = event -> x();
        oldY_ = event -> y();
        break;

      case LINE:
        this -> drawLine(oldPoint, point, pen);
        break;

      case ARROW1:
      case ARROW2:
        this -> drawArrow(oldPoint, point, pen);
        break;

      case RECT:
      case FILL_RECT:
        this -> drawRect(oldPoint, point, pen);
        break;

      case BLUR_RECT:
        this -> drawBlurRect(oldPoint, point);
        break;
      }
    }
  }

  QGraphicsView::mouseMoveEvent(event);
}

void XRapture::drawFreeLine(const QPointF& p1, const QPointF& p2, const QPen& pen)
{
  if (preDrawItem_ == 0) {
    QPainterPath path;
    path.moveTo(p2.x(), p2.y());
    path.lineTo(p1.x(), p1.y());

    auto item = this -> scene() -> addPath(path, pen);
    preDrawItem_ = item;
  }
  else {
    QGraphicsPathItem* item = static_cast<QGraphicsPathItem*>(preDrawItem_);
    auto path = item -> path();

    path.moveTo(p2.x(), p2.y());
    path.lineTo(p1.x(), p1.y());

    item -> setPath(path);
  }
}

QLineF XRapture::snapLine(const QPointF& p1, const QPointF& p2) const
{
  QLineF line(p1, p2);

  if(line.angle() > 88 && line.angle() < 92)
    line.setLine(p1.x(), p1.y(), p1.x(), p2.y());

  if(line.angle() > 178 && line.angle() < 182)
    line.setLine(p1.x(), p1.y(), p2.x(), p1.y());

  if(line.angle() > 268 && line.angle() < 272)
    line.setLine(p1.x(), p1.y(), p1.x(), p2.y());

  if(line.angle() > 358 || line.angle() < 2)
    line.setLine(p1.x(), p1.y(), p2.x(), p1.y());

  return line;
}

void XRapture::drawLine(const QPointF& p1, const QPointF& p2, const QPen& pen)
{
  if (preDrawItem_ == 0) {
    preDrawItem_ = this -> scene() -> addLine(p2.x(), p2.y(), p1.x(), p1.y(), pen);
  }
  else {
    QGraphicsLineItem* item = static_cast<QGraphicsLineItem*>(preDrawItem_);

    auto line = this -> snapLine(p1, p2);
    item -> setLine(line.x1(), line.y1(), line.x2(), line.y2());
  }
}

void XRapture::drawArrow(const QPointF& p1, const QPointF& p2, const QPen& pen)
{
  if (preDrawItem_ == 0) {
    if(drawMode_ == ARROW1) {
      preDrawItem_ = this -> scene() -> addPath(CreateArrow(p1, p2, lineWidth_), pen);
    }
    else {
      QPen npen = pen;
      npen.setWidth(1);

      preDrawItem_ = this -> scene() -> addPath(CreateArrow2(p1, p2, lineWidth_),
                                        npen, QBrush(color_));
    }
  }
  else {
    QGraphicsPathItem* item = static_cast<QGraphicsPathItem*>(preDrawItem_);

    if(drawMode_ == ARROW1) {
      item -> setPath(CreateArrow(p1, p2, lineWidth_));
    }
    else {
      item -> setPath(CreateArrow2(p1, p2, lineWidth_));
    }
  }
}

void XRapture::drawRect(const QPointF& p1, const QPointF& p2, const QPen& pen)
{
  int x = (p1.x() > p2.x()) ? p2.x() : p1.x();
  int y = (p1.y() > p2.y()) ? p2.y() : p1.y();
  int w = std::abs(p1.x() - p2.x());
  int h = std::abs(p1.y() - p2.y());

  if (preDrawItem_ == 0) {
    if(drawMode_ == RECT)
      preDrawItem_ = this -> scene() -> addRect(x, y, w, h,
                                        pen);
    else {
      QPen npen = pen;
      npen.setWidth(2);
      preDrawItem_ = this -> scene() -> addRect(x, y, w, h,
                                        npen, QBrush(color_));
    }
  }
  else {
    QGraphicsRectItem* item = static_cast<QGraphicsRectItem*>(preDrawItem_);

    item -> setRect(x, y, w, h);
  }
}

void XRapture::drawBlurRect(QPointF p1, QPointF p2)
{
  int x = (p1.x() > p2.x()) ? p2.x() : p1.x();
  int y = (p1.y() > p2.y()) ? p2.y() : p1.y();

  if(x < 0) x = 0;
  if(y < 0) y = 0;

  if(p1.x() < 0) p1.setX(0);
  if(p2.x() < 0) p2.setX(0);
  if(p1.y() < 0) p1.setY(0);
  if(p2.y() < 0) p2.setY(0);

  int w = std::abs(p1.x() - p2.x());
  int h = std::abs(p1.y() - p2.y());

  if (preDrawItem_ == 0) {
    preDrawItem_ = new QGraphicsPixmapItem();
    this -> scene() -> addItem(preDrawItem_);
  }
  QGraphicsPixmapItem* item = static_cast<QGraphicsPixmapItem*>(preDrawItem_);
  item -> setPixmap(QPixmap(0, 0));

  if (w == 0) return;
  if (h == 0) return;
  QImage img = this -> getCurrentImage(false);

  QGraphicsBlurEffect *blur = new QGraphicsBlurEffect;
  blur -> setBlurRadius(12);
  QPixmap pixmap = QPixmap::fromImage(applyEffect(QPixmap::fromImage(img),
                                                  blur, QRect(x, y, w, h)));

  item -> setPos(x, y);
  item -> setPixmap(pixmap);
}

void XRapture::wheelEvent(QWheelEvent *event)
{
  if(event -> modifiers() == Qt::ControlModifier) {
    if(event -> angleDelta().y() > 0) {
      zoomScale_ += 10;
    }
    else{
      zoomScale_ -= 10;
    }

    this -> zoomAction(zoomScale_ / 100.0);
  }
  else {
    QGraphicsView::wheelEvent(event);
  }
}

void XRapture::zoomAction(qreal scale)
{
  scale_.reset();
  scale_.scale(scale, scale);
  this -> calcTransform();
}

void XRapture::rotationAction(qreal angle)
{
  QTransform trans = rotation_;
  auto command = new TransformCommand(this, &rotation_, rotation_, trans.rotate(angle));
  undoStack_ -> push(command);
}

void XRapture::mirrorAction(bool horizontal, bool vertical)
{
  int h = 1, v = 1;

  if(horizontal) h = -1;
  if(vertical) v = -1;

  QTransform trans = mirror_;
  auto command = new TransformCommand(this, &mirror_, mirror_, trans.scale(h, v));
  undoStack_ -> push(command);
}

void XRapture::reCaptureAction()
{
  this -> setStyleSheet("#XRapture {background: transparent; border: none;}");
  this -> setGeometry(0, 0, 1, 1);

  slop::SlopSelection selection(0, 0, 0, 0, 0, true);
  slop::SlopOptions options;

  options.border = 2.0;
  options.tolerance = 0.0;

  selection = slop::SlopSelect(&options);

  if(!selection.cancelled) {
    this -> mSleep(100);

    this -> hide();
    this -> screenCapture(selection.x, selection.y, selection.w, selection.h);
  }
  this -> show();

  this -> mSleep(50);
}

void XRapture::calcTransform()
{
  this -> setTransform(scale_ * mirror_ * rotation_);

  auto trans = transform();
  auto point = trans.map(QPoint(sceneRect().width(), sceneRect().height()));
  auto rect = this -> geometry();

  this -> changeWindowGeometry(rect.x(), rect.y(), std::abs(point.x()), std::abs(point.y()));
}

void XRapture::undoAction()
{
  undoStack_ -> undo();
}

void XRapture::copyAction() const
{
  QClipboard *clipboard = QGuiApplication::clipboard();
  QMimeData *data = new QMimeData;

  auto copyImg = this -> getCurrentImage();
  data -> setImageData(copyImg);
  clipboard -> setMimeData(data);
}

void XRapture::pasteAction()
{
  const QClipboard *clipboard = QApplication::clipboard();
  const QMimeData *mimeData = clipboard -> mimeData();

  if(mimeData -> hasImage()) {
    auto img = qvariant_cast<QPixmap>(mimeData -> imageData());
    this -> setPixmap(img);

    auto rect = this -> geometry();
    this -> changeWindowGeometry(rect.x(), rect.y(), img.width(), img.height());
  }
}

void XRapture::openAction()
{
  auto fileName = QFileDialog::getOpenFileName(this);

  if(!fileName.isEmpty()) {
    QImage img(fileName);
    this -> setPixmap(QPixmap::fromImage(img));

    auto rect = this -> geometry();
    this -> changeWindowGeometry(rect.x(), rect.y(), img.width(), img.height());
  }
}

void XRapture::saveAction()
{
  auto fileName = QFileDialog::getSaveFileName(this);

  if(!fileName.isEmpty()) {
    auto saveImg = this -> getCurrentImage();
    saveImg.save(fileName);
  }
}

void XRapture::quitAction() const
{
  QApplication::quit();
}

void XRapture::mSleep(int msec) const
{
  QEventLoop loop;
  QTimer::singleShot(msec, &loop, SLOT(quit()));
  loop.exec();
}

QImage XRapture::getCurrentImage(bool trans) const
{
  auto rect = this -> sceneRect();
  QImage img(rect.width(), rect.height(), QImage::Format_RGB32);
  QPainter painter(&img);
  painter.setRenderHints( QPainter::Antialiasing | QPainter::HighQualityAntialiasing );
  this -> scene() -> render(&painter, rect, rect);

  if(trans)
    return img.transformed(mirror_ * rotation_);
  else
    return img;
}
