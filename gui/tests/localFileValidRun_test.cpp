#include "mainwindow.h"
#include <QtTest/QtTest>

class localFileValidRun: public QObject
{
    Q_OBJECT

private slots:
    void testLocalFileValidRun();

};

void localFileValidRun::testLocalFileValidRun()
{


}

QTEST_MAIN(localFileValidRun)
#include "localFileValidRun_test.moc"
