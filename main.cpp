#include <QApplication>
#include <QSplashScreen>
#include <QPixmap>
#include <QTimer>
#include <QEventLoop>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 加载并缩放启动图片
    QPixmap pixmap(":/img/openPicture.png");
    pixmap = pixmap.scaled(500, 500, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QSplashScreen splash(pixmap);
    splash.show();

    a.processEvents();

    // 等待 2 秒
    QEventLoop loop;
    QTimer::singleShot(1000, &loop, &QEventLoop::quit);
    loop.exec();

    MainWindow w;
    w.show();

    splash.finish(&w);

    return a.exec();
}