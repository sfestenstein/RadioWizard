#include "MainWindow.h"

#include "GeneralLogger.h"

#include <QApplication>

int main(int argc, char* argv[])
{
   CommonUtils::GeneralLogger logger;
   logger.init("RadioWizardMain");

   const QApplication a(argc, argv);
   MainWindow w;
   w.show();
   return QApplication::exec();
}

