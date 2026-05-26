#include "project/main_window.h"
#include "project/logger.h"
#include <QApplication>
#include <QTranslator>
#include <QLibraryInfo>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 加载 Qt 中文翻译，让内置对话框（QFontDialog 等）显示中文
    QTranslator *qtTranslator = new QTranslator(&a);
    if (qtTranslator->load("qt_zh_CN", QLibraryInfo::location(QLibraryInfo::TranslationsPath))) {
        a.installTranslator(qtTranslator);
    }
    QTranslator *qtBaseTranslator = new QTranslator(&a);
    if (qtBaseTranslator->load("qtbase_zh_CN", QLibraryInfo::location(QLibraryInfo::TranslationsPath))) {
        a.installTranslator(qtBaseTranslator);
    }

    AppLogger::instance().info("程序启动");

    MainWindow w;
    w.show();

    int ret = a.exec();
    AppLogger::instance().info("程序退出，返回码: " + QString::number(ret));
    return ret;
}
