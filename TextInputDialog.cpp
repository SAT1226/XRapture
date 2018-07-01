#include <TextInputDialog.hpp>
#include <QPainter>
#include <QVBoxLayout>
#include <QPlainTextEdit>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QFontDialog>
#include <QColorDialog>
#include <QCheckBox>

TextInputDialog::TextInputDialog(QColor color, QWidget *parent) : QDialog(parent)
{
  color_ = color;
  outline_ = false;
  outlineColor_ = Qt::white;
  QVBoxLayout* baseLayout = new QVBoxLayout(this);
  QHBoxLayout* buttonLayout = new QHBoxLayout();

  textEdit_ = new QPlainTextEdit(this);
  baseLayout -> addWidget(textEdit_);

  QPushButton* fontButton = new QPushButton(font_.family() + ":" + QString::number(font_.pointSize()));

  buttonLayout -> addWidget(fontButton);
  connect(fontButton, &QPushButton::clicked,
          [=] {
            bool ok;
            QFont font = QFontDialog::getFont(&ok, font_, this);
            if(ok) {
              font_ = font;
              fontButton -> setText(font_.family() + ":" + QString::number(font_.pointSize()));
            }
          });

  QPushButton* colorButton = new QPushButton("");
  buttonLayout -> addWidget(colorButton);

  colorButton -> setIcon(QIcon(CreateColorPixmap(color_)));
  connect(colorButton, &QPushButton::clicked,
          [=] {
            int alpha = color_.alpha();

            auto color = QColorDialog::getColor(color_, this);
            if(color.isValid()) color_ = color;

            color_.setAlpha(alpha);

            colorButton -> setIcon(QIcon(CreateColorPixmap(color_)));
          });

  outlineCheckBox_ = new QCheckBox("Outline");
  QPushButton* outlineColorButton = new QPushButton("");
  outlineColorButton -> setDisabled(true);
  buttonLayout -> addWidget(outlineCheckBox_);
  connect(outlineCheckBox_, &QCheckBox::toggled,
          [=] (bool visible) {
            outlineColorButton -> setDisabled(!visible);
            outline_ = visible;
          });

  buttonLayout -> addWidget(outlineColorButton);

  outlineColorButton -> setIcon(QIcon(CreateColorPixmap(outlineColor_)));
  buttonLayout -> addWidget(outlineColorButton);
  connect(outlineColorButton, &QPushButton::clicked,
          [=] {
            int alpha = color_.alpha();

            auto color = QColorDialog::getColor(outlineColor_, this);
            if(color.isValid()) outlineColor_ = color;

            color_.setAlpha(alpha);

            outlineColorButton -> setIcon(QIcon(CreateColorPixmap(outlineColor_)));
          });

  QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok |
                                                     QDialogButtonBox::Cancel,
                                                     Qt::Horizontal);
  connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  buttonLayout -> addWidget(buttonBox);

  baseLayout -> addLayout(buttonLayout);
  this -> setLayout(baseLayout);
}

void TextInputDialog::accept() {
  text_ = textEdit_ -> toPlainText();
  QDialog::accept();
}

QColor TextInputDialog::getColor() {
  return color_;
}

QFont TextInputDialog::getFont() {
  return font_;
}

QString TextInputDialog::getText() {
  return text_;
}

bool TextInputDialog::getOutline() {
  return outline_;
}

QColor TextInputDialog::getOutlineColor() {
  return outlineColor_;
}

QPixmap TextInputDialog::CreateColorPixmap(const QColor& color)
{
  QPixmap pixmap(100, 100);
  pixmap.fill(color);

  QPainter painter(&pixmap);
  painter.setPen(QColor(100,100,100,255));
  painter.drawRect(0, 0, pixmap.height() - 1, pixmap.width() - 1);

  return pixmap;
}
