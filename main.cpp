#include <QApplication>
#include <iostream>
#include <slop.hpp>

#include "XRapture.hpp"

int main(int argc, char** argv)
{
  slop::SlopSelection selection(0, 0, 0, 0, 0, true);
  slop::SlopOptions options;
  options.border = 2.0;
  options.tolerance = 0.0;

  if(argc == 1) {
    selection = slop::SlopSelect(&options);
    if(selection.cancelled) {
      std::cerr << "cancelled" << std::endl;
      return 1;
    }
  }

  QApplication app(argc, argv);
  QGraphicsScene scene;
  XRapture* xrapture = new XRapture(&scene);

  if(argc > 1) {
    xrapture -> show();
    if(!xrapture -> openImageFile(argv[1])) return 1;
  }
  else {
    xrapture -> screenCapture(selection.x, selection.y, selection.w, selection.h);
    xrapture -> show();
  }

  return app.exec();
}
