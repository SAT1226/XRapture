#include <QGuiApplication>
#include <QGraphicsView>
#include <QMouseEvent>
#include <QScreen>
#include <QStack>

class QUndoStack;
class QMenu;
class TransformCommand;
class XRapture: public QGraphicsView
{
public:
  friend TransformCommand;
  XRapture(QGraphicsScene* scene);

  void setPixmap(QPixmap pixmap);
  void keyPressEvent(QKeyEvent *event);
  void mousePressEvent(QMouseEvent* event);
  void mouseReleaseEvent(QMouseEvent* event);
  void mouseMoveEvent(QMouseEvent* event);
  void wheelEvent(QWheelEvent *event);
  void contextMenuEvent(QContextMenuEvent *event);
  void screenCapture(int x, int y, int w, int h);
  void reCaptureAction();
  bool openImageFile(const QString fileName);

private:
  QImage applyEffect(const QPixmap& src, QGraphicsEffect *effect, const QRect& rect) const;
  QPixmap CreateColorPixmap(const QColor& color) const;
  QPainterPath CreateArrow(const QPointF& p1, const QPointF& p2, int width) const;
  QPainterPath CreateArrow2(const QPointF& p1, const QPointF& p2, int width) const;

  void quitAction() const;
  void openAction();
  void saveAction();
  void zoomAction(qreal scale);
  void rotationAction(qreal angle);
  void mirrorAction(bool horizontal, bool vertical);
  void undoAction();
  void copyAction() const;
  void pasteAction();

  void createEditSubMenu(QMenu* menu);
  void createColorSubMenu(QMenu* menu);
  void createLineWidthSubMenu(QMenu* menu);
  void createDrawSubMenu(QMenu* menu, QContextMenuEvent *event);
  void createTransformSubMenu(QMenu* menu);
  void createOpacitySubMenu(QMenu* menu);
  void createZoomSubMenu(QMenu* menu);

  void calcTransform();
  void changeWindowGeometry(int x, int y, int w, int h);

  void drawFreeLine(const QPointF& p1, const QPointF& p2, const QPen& pen);
  void drawLine(const QPointF& p1, const QPointF& p2, const QPen& pen);
  void drawArrow(const QPointF& p1, const QPointF& p2, const QPen& pen);
  void drawRect(const QPointF& p1, const QPointF& p2, const QPen& pen);
  void drawBlurRect(QPointF p1, QPointF p2);

  QLineF snapLine(const QPointF& p1, const QPointF& p2) const;
  void mSleep(int msec) const;
  QImage getCurrentImage(bool trans = false) const;

  bool titleBar_;
  bool textMode_;
  int zoomScale_;
  QTransform scale_;
  QTransform mirror_;
  QTransform rotation_;
  QUndoStack *undoStack_;
  QColor color_;
  int lineWidth_;
  bool highlighter_;
  QGraphicsPixmapItem* pixmap_;
  QGraphicsItem* preDrawItem_;
  Qt::MouseButton oldButton_;
  Qt::KeyboardModifiers oldMouseModifiers_;
  int oldX_, oldY_;

  enum DrawMode {
    FREE_LINE,
    LINE,
    ARROW1,
    ARROW2,
    RECT,
    FILL_RECT,
    BLUR_RECT,
  };
  DrawMode drawMode_;
};
