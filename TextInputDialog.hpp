#ifndef TEXTINPUTDIALOG_H
#define TEXTINPUTDIALOG_H
#include <QDialog>

class QPlainTextEdit;
class QCheckBox;
class TextInputDialog : public QDialog
{
public:
  TextInputDialog(QColor color, QWidget *parent = Q_NULLPTR);

  void accept();
  QColor getColor();
  QFont getFont();
  QString getText();
  bool getOutline();
  QColor getOutlineColor();

private:
  QPixmap CreateColorPixmap(const QColor& color);

  QPlainTextEdit* textEdit_;
  QCheckBox* outlineCheckBox_;

  bool outline_;
  QColor color_, outlineColor_;
  QFont font_;
  QString text_;
};
#endif /* TEXTINPUTDIALOG_H */
