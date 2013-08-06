/*
    Copyright (C) 2013  Hong Jen yee (PCMan) <pcman.tw@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef LIBLXQT_SETTINGS_H
#define LIBLXQT_SETTINGS_H

#include <QGlobalStatic>
#include <QObject>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QSettings>

namespace LxQt
{

class SettingsPrivate;

class Settings: public QObject
{
    Q_OBJECT

public:
    explicit Settings(QObject *parent = 0); // Uses QCoreApplication
    explicit Settings(const QString &organization, const QString &application,
                      QObject *parent = 0);
    ~Settings();

    void clear(); // does nothing
    void sync();
    QSettings::Status status() const;

    void beginGroup(const QString &prefix);
    void endGroup();
    QString group() const;

    int beginReadArray(const QString& prefix);
    void beginWriteArray(const QString& prefix, int size = -1);
    void endArray();
    void setArrayIndex(int i);

    QStringList allKeys() const;
    QStringList childKeys() const;
    QStringList childGroups() const;
    bool isWritable() const;

    void setValue(const QString &key, const QVariant &value);
    QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const;

    void remove(const QString &key);
    bool contains(const QString &key) const;

    QString organizationName() const;
    QString applicationName() const;

private:
    Q_DISABLE_COPY(Settings)
    SettingsPrivate *d_ptr;
    Q_DECLARE_PRIVATE_D(d_ptr, Settings)
};

} // namespace LxQt

#endif // LIBLXQT_SETTINGS_H
