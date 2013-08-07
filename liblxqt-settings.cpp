/*
    Copyright (C) 2013  Hong Jen yee (PCMan) <pcman.tw@gmail.com>
    Copyright (C) 2013  Digia Plc and/or its subsidiary(-ies).

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

#include "liblxqt-settings.h"

#include <QCoreApplication>
#include <QStack>
#include <QRegExp>
#include <QSize>
#include <QPoint>
#include <QRect>

#include <QDebug>

extern "C" { // dconf does not do extern "C" properly in its header
#include <dconf/dconf.h>
}

namespace LxQt {

static QString variantToString(const QVariant &v);
static QVariant stringToVariant(const QString &s);
static QStringList splitArgs(const QString &s, int idx);

class SettingsPrivate
{
    Settings *q_ptr;
    Q_DECLARE_PUBLIC(Settings)

public:
    SettingsPrivate(const QString &organization, const QString &application);
    ~SettingsPrivate();

    void clear();
    void sync();
    QSettings::Status status() const;

    void beginGroup(const QString &prefix);
    void endGroup();
    QString group() const;

    int beginReadArray(const QString& prefix);
    void beginWriteArray(const QString& prefix);
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
    DConfClient *m_client;
    QString m_organizationName;
    QString m_applicationName;
    QStringList m_path;
    QString m_currentPath;
    QStack<bool> m_groups;

    static void c_dconfChanged(DConfClient *client, gchar *prefix, GStrv changes, gchar *tag, gpointer user_data);
    void dconfChanged(gchar *prefix, GStrv changes, gchar *tag);

    static QString normalisedPath(const QString &path);
    QStringList allKeys(const char *path, const QString &prefix) const;
};

SettingsPrivate::SettingsPrivate(const QString &organization, const QString &application)
    : m_client(0)
    , m_organizationName(organization)
    , m_applicationName(application)
{
    m_currentPath = m_organizationName;
    if (!m_applicationName.isEmpty())
    {
        m_currentPath += QLatin1String("/") + m_applicationName;
    }
    m_currentPath = QLatin1String("/") + normalisedPath(m_currentPath);
    m_path << m_currentPath;
    m_currentPath += QLatin1Char('/');

// not sure if this condition should be compile-time:
#if (G_ENCODE_VERSION (GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION)) < GLIB_VERSION_2_36
    g_type_init();
#endif
    m_client = dconf_client_new();

    if (m_client)
    {
        g_signal_connect(m_client, "changed", G_CALLBACK(c_dconfChanged), this);
        dconf_client_watch_fast(m_client, m_currentPath.toLatin1().constData());
    }
}

SettingsPrivate::~SettingsPrivate()
{
    if (m_client)
    {
        dconf_client_unwatch_fast(m_client, (m_path[0] + QLatin1Char('/')).toLatin1().constData());
        g_signal_handlers_disconnect_by_data(m_client, this);
        g_object_unref(m_client);
    }
}

void SettingsPrivate::c_dconfChanged(DConfClient *client, gchar *prefix, GStrv changes, gchar *tag, gpointer user_data)
{
    Q_UNUSED(client);
    static_cast<SettingsPrivate*>(user_data)->dconfChanged(prefix, changes, tag);
}

void SettingsPrivate::dconfChanged(gchar *prefix, GStrv changes, gchar *tag)
{
    qDebug() << "dconfChanged(), prefix: " << prefix;
    for (char **change = changes; *change; ++change)
        qDebug() << "dconfChanged(), change: " << *change;
    qDebug() << "dconfChanged(), tag: " << tag;

    if (!m_path.isEmpty())
    {
        QString qPrefix(prefix);
        if (qPrefix.length() > m_path[0].length() + 1)
        {
            qPrefix = qPrefix.mid(m_path[0].length() + 1);
            qDebug() << "dconfChanged(), qPrefix: " << qPrefix;
            Q_Q(Settings);
            Q_EMIT q->changed(qPrefix);
        }
    }
}

QString SettingsPrivate::normalisedPath(const QString &path)
{
    QString normalisedPrefix = path;
    static QRegExp rx(QLatin1String("(^/+)|(//+)|(/+$)"));
    if (normalisedPrefix.indexOf(rx) != -1) // alternative condition: (normalisedPrefix.startsWith(QLatin1Char('/')) || normalisedPrefix.endsWith(QLatin1Char('/')) || (normalisedPrefix.indexOf(QLatin1String("//")) != -1))
        normalisedPrefix = normalisedPrefix.remove(rx);
    return normalisedPrefix;
}

void SettingsPrivate::clear()
{
    qDebug() << "clear(), path: " << m_currentPath;
    dconf_client_write_sync(m_client, m_currentPath.toLatin1().constData(), NULL, NULL, NULL, NULL);
}

void SettingsPrivate::sync()
{
    dconf_client_sync(m_client);
}

QSettings::Status SettingsPrivate::status() const
{
    return QSettings::NoError; // FIXME: should we omit errors?
}

void SettingsPrivate::beginGroup(const QString &prefix)
{
    QString path = normalisedPath(prefix);

    m_path += path;
    m_currentPath = m_path.join(QLatin1String("/")) + QLatin1Char('/');
    m_groups.push(true);

    qDebug() << "beginGroup, m_currentPath: " << m_currentPath;
}

void SettingsPrivate::endGroup()
{
    if (m_groups.isEmpty())
    {
        qWarning() << "endGroup() called with no groups";
        return;
    }

    if (!m_groups.top())
    {
        qWarning() << "endGroup() called on an array";
        return;
    }

    if (m_path.length() <= 1)
    {
        qWarning() << "endArray() called on a corrupted path";
        return;
    }

    m_path.removeLast();
    m_currentPath = m_path.join(QLatin1String("/")) + QLatin1Char('/');
    m_groups.pop();

    qDebug() << "endGroup, m_currentPath: " << m_currentPath;
}

QString SettingsPrivate::group() const
{
    return QStringList(m_path.mid(1)).join(QLatin1String("/"));
}

int SettingsPrivate::beginReadArray(const QString &prefix)
{
    QString path = normalisedPath(prefix);

    m_path += path;

    int result = 0;
    Q_FOREACH (QString group, childGroups())
    {
        bool ok;
        int index = group.toInt(&ok);
        if (ok)
            result = index + 1;
    }

    m_path += QLatin1String("0");
    m_currentPath = m_path.join(QLatin1String("/")) + QLatin1Char('/');
    m_groups.push(false);

    qDebug() << "beginReadArray, m_currentPath: " << m_currentPath;

    return result;
}

void SettingsPrivate::beginWriteArray(const QString &prefix)
{
    QString path = normalisedPath(prefix);

    m_path += path;
    m_path += QLatin1String("0");
    m_currentPath = m_path.join(QLatin1String("/")) + QLatin1Char('/');
    m_groups.push(false);

    qDebug() << "beginWriteArray, m_currentPath: " << m_currentPath;
}

void SettingsPrivate::endArray()
{
    if (m_groups.isEmpty())
    {
        qWarning() << "endArray() called with no arrays";
        return;
    }

    if (!m_groups.top())
    {
        qWarning() << "endArray() called on a group";
        return;
    }

    if (m_path.length() <= 2)
    {
        qWarning() << "endArray() called on a corrupted path";
        return;
    }

    m_path.removeLast();
    m_path.removeLast();
    m_currentPath = m_path.join(QLatin1String("/")) + QLatin1Char('/');
    m_groups.pop();

    qDebug() << "endGroup, m_currentPath: " << m_currentPath;
}

void SettingsPrivate::setArrayIndex(int i)
{
    if (m_groups.isEmpty())
    {
        qWarning() << "setArrayIndex() called with no arrays";
        return;
    }

    if (!m_groups.top())
    {
        qWarning() << "setArrayIndex() called on a group";
        return;
    }

    if (m_path.length() <= 2)
    {
        qWarning() << "setArrayIndex() called on a corrupted path";
        return;
    }

    m_path.removeLast();
    m_path += QString::number(i);
    m_currentPath = m_path.join(QLatin1String("/")) + QLatin1Char('/');

    qDebug() << "endGroup, m_currentPath: " << m_currentPath;
}

QStringList SettingsPrivate::allKeys() const
{
    dconf_client_sync(m_client);

    QString prefix = group();
    if (!prefix.isEmpty())
    {
        prefix += QLatin1Char('/');
    }
    return allKeys(m_currentPath.toLatin1().constData(), prefix);
}

QStringList SettingsPrivate::allKeys(const char *path, const QString &prefix) const
{
    QStringList result;

    char **keys = dconf_client_list(m_client, path, NULL);

    if (keys)
    {
        for (char **key = keys; *key; ++key)
        {
            if (dconf_is_rel_dir(*key, NULL))
            {
                result += allKeys((QString(path) + *key).toLatin1().constData(), prefix + *key);
            }
            else if (dconf_is_rel_key(*key, NULL))
            {
                result += prefix + *key;
            }
        }

        g_strfreev(keys);
    }

    return result;
}

QStringList SettingsPrivate::childKeys() const
{
    dconf_client_sync(m_client);

    QStringList result;

    char **keys = dconf_client_list(m_client, m_currentPath.toLatin1().constData(), NULL);

    if (keys)
    {
        for (char **key = keys; *key; ++key)
        {
            if (dconf_is_rel_key(*key, NULL))
            {
                result += *key;
            }
        }

        g_strfreev(keys);
    }

    return result;
}

QStringList SettingsPrivate::childGroups() const
{
    dconf_client_sync(m_client);

    QStringList result;

    char **keys = dconf_client_list(m_client, m_currentPath.toLatin1().constData(), NULL);

    if (keys)
    {
        for (char **key = keys; *key; ++key)
        {
            if (dconf_is_rel_dir(*key, NULL))
            {
                result += *key;
            }
        }

        g_strfreev(keys);
    }

    return result;
}

bool SettingsPrivate::isWritable() const
{
    return bool(dconf_client_is_writable(m_client, m_currentPath.toLatin1().constData()));
}

void SettingsPrivate::setValue(const QString &key, const QVariant &value)
{
    GError *err = NULL;
    QString str = variantToString(value);
    QString path = m_currentPath + normalisedPath(key);
    GVariant *val = g_variant_new_string(str.toUtf8().constData());
    g_variant_ref_sink(val);
    qDebug() << "setValue: path: " << path << ", value: " << g_variant_get_string(val, NULL);
    dconf_client_write_fast(m_client, path.toLatin1().constData(), val, &err);
    if (val)
    {
        g_variant_unref(val);
    }

    if (err)
    {
        qDebug() << "error: " << err->message;
        g_error_free(err);
    }
}

QVariant SettingsPrivate::value(const QString &key, const QVariant &defaultValue) const
{
    dconf_client_sync(m_client);

    QString path = m_currentPath + normalisedPath(key);
    qDebug() << "value(), path: " << path;
    GVariant *val = dconf_client_read(m_client, path.toLatin1().constData());
    if (val)
    {
        const char *str = g_variant_get_string(val, NULL);
        if (str)
        {
            QVariant qv = stringToVariant(str);
            // g_variant_unref(val);
            return qv;
        }
        // g_variant_unref(val);
    }
    return defaultValue;
}

void SettingsPrivate::remove(const QString &key)
{
    QString path = m_currentPath + normalisedPath(key);
    qDebug() << "remove(), path: " << path;
    dconf_client_write_sync(m_client, path.toLatin1().constData(), NULL, NULL, NULL, NULL);
}

bool SettingsPrivate::contains(const QString &key) const
{
    dconf_client_sync(m_client);

    QString path = m_currentPath + normalisedPath(key);
    qDebug() << "contains(), path: " << path;
    return dconf_client_read(m_client, path.toLatin1().constData()) != NULL;
}

QString SettingsPrivate::organizationName() const
{
    return m_organizationName;
}

QString SettingsPrivate::applicationName() const
{
    return m_applicationName;
}

// taken from Qt source code (src/corelib/io/qsettings.cpp).
// Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
// license: LGPL
static QString variantToString(const QVariant &v)
{
    QString result;

    switch (v.type())
    {
    case QVariant::Invalid:
        result = QLatin1String("@Invalid()");
        break;

    case QVariant::ByteArray:
    {
        QByteArray a = v.toByteArray();
        result = QLatin1String("@ByteArray(");
        result += QString::fromLatin1(a.constData(), a.size());
        result += QLatin1Char(')');
        break;
    }

    case QVariant::String:
    case QVariant::LongLong:
    case QVariant::ULongLong:
    case QVariant::Int:
    case QVariant::UInt:
    case QVariant::Bool:
    case QVariant::Double:
    case QVariant::KeySequence:
    {
        result = v.toString();

        if (result.startsWith(QLatin1Char('@')))
        {
            result.prepend(QLatin1Char('@'));
        }

        break;
    }
#ifndef QT_NO_GEOM_VARIANT
    case QVariant::Rect:
    {
        QRect r = qvariant_cast<QRect>(v);
        result += QLatin1String("@Rect(");
        result += QString::number(r.x());
        result += QLatin1Char(' ');
        result += QString::number(r.y());
        result += QLatin1Char(' ');
        result += QString::number(r.width());
        result += QLatin1Char(' ');
        result += QString::number(r.height());
        result += QLatin1Char(')');
        break;
    }
    case QVariant::Size:
    {
        QSize s = qvariant_cast<QSize>(v);
        result += QLatin1String("@Size(");
        result += QString::number(s.width());
        result += QLatin1Char(' ');
        result += QString::number(s.height());
        result += QLatin1Char(')');
        break;
    }
    case QVariant::Point:
    {
        QPoint p = qvariant_cast<QPoint>(v);
        result += QLatin1String("@Point(");
        result += QString::number(p.x());
        result += QLatin1Char(' ');
        result += QString::number(p.y());
        result += QLatin1Char(')');
        break;
    }
#endif // !QT_NO_GEOM_VARIANT

    default:
    {
#ifndef QT_NO_DATASTREAM
        QByteArray a;
        {
            QDataStream s(&a, QIODevice::WriteOnly);
            s.setVersion(QDataStream::Qt_4_0);
            s << v;
        }

        result = QLatin1String("@Variant(");
        result += QString::fromLatin1(a.constData(), a.size());
        result += QLatin1Char(')');
#else
        Q_ASSERT(!"QSettings: Cannot save custom types without QDataStream support");
#endif
        break;
    }
    }

    return result;
}


// taken from Qt source code (src/corelib/io/qsettings.cpp).
// Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
// license: LGPL
static QVariant stringToVariant(const QString &s)
{
    if (s.startsWith(QLatin1Char('@')))
    {
        if (s.endsWith(QLatin1Char(')')))
        {
            if (s.startsWith(QLatin1String("@ByteArray(")))
            {
                return QVariant(s.toLatin1().mid(11, s.size() - 12));
            }
            else if (s.startsWith(QLatin1String("@Variant(")))
            {
#ifndef QT_NO_DATASTREAM
                QByteArray a(s.toLatin1().mid(9));
                QDataStream stream(&a, QIODevice::ReadOnly);
                stream.setVersion(QDataStream::Qt_4_0);
                QVariant result;
                stream >> result;
                return result;
#else
                Q_ASSERT(!"QSettings: Cannot load custom types without QDataStream support");
#endif
#ifndef QT_NO_GEOM_VARIANT
            }
            else if (s.startsWith(QLatin1String("@Rect(")))
            {
                QStringList args = splitArgs(s, 5);

                if (args.size() == 4)
                {
                    return QVariant(QRect(args[0].toInt(), args[1].toInt(), args[2].toInt(), args[3].toInt()));
                }
            }
            else if (s.startsWith(QLatin1String("@Size(")))
            {
                QStringList args = splitArgs(s, 5);

                if (args.size() == 2)
                {
                    return QVariant(QSize(args[0].toInt(), args[1].toInt()));
                }
            }
            else if (s.startsWith(QLatin1String("@Point(")))
            {
                QStringList args = splitArgs(s, 6);

                if (args.size() == 2)
                {
                    return QVariant(QPoint(args[0].toInt(), args[1].toInt()));
                }

#endif
            }
            else if (s == QLatin1String("@Invalid()"))
            {
                return QVariant();
            }

        }

        if (s.startsWith(QLatin1String("@@")))
        {
            return QVariant(s.mid(1));
        }
    }

    return QVariant(s);
}

// taken from Qt source code (src/corelib/io/qsettings.cpp).
// Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
// license: LGPL
static QStringList splitArgs(const QString &s, int idx)
{
    int l = s.length();
    Q_ASSERT(l > 0);
    Q_ASSERT(s.at(idx) == QLatin1Char('('));
    Q_ASSERT(s.at(l - 1) == QLatin1Char(')'));

    QStringList result;
    QString item;

    for (++idx; idx < l; ++idx)
    {
        QChar c = s.at(idx);
        if (c == QLatin1Char(')'))
        {
            Q_ASSERT(idx == l - 1);
            result.append(item);
        }
        else if (c == QLatin1Char(' '))
        {
            result.append(item);
            item.clear();
        }
        else
        {
            item.append(c);
        }
    }

    return result;
}



Settings::Settings(QObject *parent)
    : QObject(parent)
    , d_ptr(new SettingsPrivate(
#ifdef Q_OS_MAC
        QCoreApplication::organizationDomain().isEmpty()
            ? (QLatin1String("org/") + QCoreApplication::organizationName())
            : QCoreApplication::organizationDomain().replace(QLatin1Char('.'), QLatin1Char('/')) ,
#else
        QCoreApplication::organizationName().isEmpty()
            ? QCoreApplication::organizationDomain().replace(QLatin1Char('.'), QLatin1Char('/'))
            : (QLatin1String("org/") + QCoreApplication::organizationName()) ,
#endif
        QCoreApplication::applicationName()))
{
}

Settings::Settings(const QString &organization, const QString &application, QObject *parent)
    : QObject(parent)
    , d_ptr(new SettingsPrivate(QLatin1String("org/") + organization, application))
{
}

Settings::~Settings()
{
    sync();
    delete d_ptr;
}

void Settings::clear()
{
    Q_D(Settings);
    d->clear();
}

void Settings::sync()
{
    Q_D(Settings);
    d->sync();
}

QSettings::Status Settings::status() const
{
    Q_D(const Settings);
    return d->status();
}

void Settings::beginGroup(const QString &prefix)
{
    Q_D(Settings);
    d->beginGroup(prefix);
}

void Settings::endGroup()
{
    Q_D(Settings);
    d->endGroup();
}

QString Settings::group() const
{
    Q_D(const Settings);
    return d->group();
}

int Settings::beginReadArray(const QString& prefix)
{
    Q_D(Settings);
    return d->beginReadArray(prefix);
}

void Settings::beginWriteArray(const QString& prefix)
{
    Q_D(Settings);
    return d->beginWriteArray(prefix);
}

void Settings::endArray()
{
    Q_D(Settings);
    return d->endArray();
}

void Settings::setArrayIndex(int i)
{
    Q_D(Settings);
    return d->setArrayIndex(i);
}

QStringList Settings::allKeys() const
{
    Q_D(const Settings);
    return d->allKeys();
}

QStringList Settings::childKeys() const
{
    Q_D(const Settings);
    return d->childKeys();
}

QStringList Settings::childGroups() const
{
    Q_D(const Settings);
    return d->childGroups();
}

bool Settings::isWritable() const
{
    Q_D(const Settings);
    return d->isWritable();
}

void Settings::setValue(const QString &key, const QVariant &value)
{
    Q_D(Settings);
    d->setValue(key, value);
}

QVariant Settings::value(const QString &key, const QVariant &defaultValue) const
{
    Q_D(const Settings);
    return d->value(key, defaultValue);
}

void Settings::remove(const QString &key)
{
    Q_D(Settings);
    d->remove(key);
}

bool Settings::contains(const QString &key) const
{
    Q_D(const Settings);
    return d->contains(key);
}

QString Settings::organizationName() const
{
    Q_D(const Settings);
    return d->organizationName();
}

QString Settings::applicationName() const
{
    Q_D(const Settings);
    return d->applicationName();
}

} // namespace LxQt
