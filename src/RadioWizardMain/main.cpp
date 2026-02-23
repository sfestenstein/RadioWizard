#include "MainWindow.h"

#include "GeneralLogger.h"
#include "StackTrace.h"

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>

int main(int argc, char* argv[])
{
   CommonUtils::GeneralLogger logger;
   logger.init("RadioWizardMain");

   // Install crash signal handlers with a best-effort hook that flushes
   // the spdlog loggers before the process terminates.
   CommonUtils::StackTrace::setPostCrashHook([](int sig)
   {
      GPCRIT("Caught fatal signal {} — see stderr for stack trace", sig);
      if (CommonUtils::GeneralLogger::s_traceLogger)
      {
         CommonUtils::GeneralLogger::s_traceLogger->dump_backtrace();
         CommonUtils::GeneralLogger::s_traceLogger->flush();
      }
      if (CommonUtils::GeneralLogger::s_generalLogger)
      {
         CommonUtils::GeneralLogger::s_generalLogger->flush();
      }
   });
   CommonUtils::StackTrace::installSignalHandlers();

   const QApplication a(argc, argv);

   // Apply a dark color palette to the entire application.
   QApplication::setStyle(QStyleFactory::create("Fusion"));
   QPalette darkPalette;
   darkPalette.setColor(QPalette::Window,           QColor(45, 45, 48));
   darkPalette.setColor(QPalette::WindowText,       QColor(208, 208, 208));
   darkPalette.setColor(QPalette::Base,             QColor(30, 30, 30));
   darkPalette.setColor(QPalette::AlternateBase,    QColor(45, 45, 48));
   darkPalette.setColor(QPalette::ToolTipBase,      QColor(60, 60, 65));
   darkPalette.setColor(QPalette::ToolTipText,      QColor(208, 208, 208));
   darkPalette.setColor(QPalette::Text,             QColor(208, 208, 208));
   darkPalette.setColor(QPalette::Button,           QColor(55, 55, 58));
   darkPalette.setColor(QPalette::ButtonText,       QColor(208, 208, 208));
   darkPalette.setColor(QPalette::BrightText,       QColor(255, 51, 51));
   darkPalette.setColor(QPalette::Link,             QColor(42, 130, 218));
   darkPalette.setColor(QPalette::Highlight,        QColor(42, 130, 218));
   darkPalette.setColor(QPalette::HighlightedText,  QColor(240, 240, 240));
   darkPalette.setColor(QPalette::PlaceholderText,  QColor(120, 120, 120));

   // Disabled state
   darkPalette.setColor(QPalette::Disabled, QPalette::WindowText,  QColor(127, 127, 127));
   darkPalette.setColor(QPalette::Disabled, QPalette::Text,        QColor(127, 127, 127));
   darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText,  QColor(127, 127, 127));
   darkPalette.setColor(QPalette::Disabled, QPalette::Highlight,   QColor(80, 80, 80));
   darkPalette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(127, 127, 127));
   QApplication::setFont(QFont("Roboto", 12)); 
   QApplication::setPalette(darkPalette);

   MainWindow w;
   w.show();
   return QApplication::exec();
}

