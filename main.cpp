#include <QCoreApplication>
#include "liblxqt-settings.h"

#include <QDebug>


int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName("lxde");
    app.setApplicationName("setting_test");

    {
        LxQt::Settings settings;

        settings.beginGroup("testGroup");
        QVariant v = settings.value("Test");
        qDebug("value: %d", v.isValid());
        settings.endGroup();

        settings.beginGroup("testGroup2");
        settings.setValue("TestStr", "Test");
        settings.setValue("TestInt", 123);
        settings.setValue("TestBool", true);
        settings.setValue("TestFloat", 123.45);
        qDebug() << "group: " << settings.group();
        settings.endGroup();
        settings.beginGroup("/testGroup2/testGroup3/");
        qDebug() << "group: " << settings.group();
        settings.setValue("TestStr", "Test2");
        settings.endGroup();

        settings.sync();

        Q_FOREACH (QString key, settings.allKeys())
            qDebug() << "key: " << key;

        settings.beginGroup("testGroup2");
        Q_FOREACH (QString key, settings.allKeys())
            qDebug() << "testGroup2/key: " << key;
        qDebug() <<"read TestStr: " << settings.value("TestStr").toString();
        qDebug() <<"read TestInt: " << settings.value("TestInt").toInt();
        qDebug() <<"read TestBool: " << settings.value("TestBool").toBool();
        qDebug() <<"read TestFloat: " << settings.value("TestFloat").toFloat();
        qDebug() <<"read testGroup3/TestStr: " << settings.value("testGroup3/TestStr").toString();
        settings.endGroup();
    }

    {
        QSettings settings;

        settings.beginGroup("testGroup");
        QVariant v = settings.value("Test");
        qDebug("value: %d", v.isValid());
        settings.endGroup();

        settings.beginGroup("testGroup2");
        settings.setValue("TestStr", "Test");
        settings.setValue("TestInt", 123);
        settings.setValue("TestBool", true);
        settings.setValue("TestFloat", 123.45);
        qDebug() << "group: " << settings.group();
        settings.endGroup();
        settings.beginGroup("/testGroup2/testGroup3/");
        qDebug() << "group: " << settings.group();
        settings.setValue("TestStr", "Test2");
        settings.endGroup();

        settings.sync();

        Q_FOREACH (QString key, settings.allKeys())
            qDebug() << "key: " << key;

        settings.beginGroup("testGroup2");
        Q_FOREACH (QString key, settings.allKeys())
            qDebug() << "testGroup2/key: " << key;
        qDebug() <<"read TestStr: " << settings.value("TestStr").toString();
        qDebug() <<"read TestInt: " << settings.value("TestInt").toInt();
        qDebug() <<"read TestBool: " << settings.value("TestBool").toBool();
        qDebug() <<"read TestFloat: " << settings.value("TestFloat").toFloat();
        qDebug() <<"read testGroup3/TestStr: " << settings.value("testGroup3/TestStr").toString();
        settings.endGroup();
    }

//  return app.exec();
}
