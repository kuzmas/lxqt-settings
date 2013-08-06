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
#include <QSize>
#include <QPoint>
#include <QRect>

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
    DConfClient *m_client;
    QString m_organizationName;
    QString m_applicationName;
    QByteArray m_prefix; // QStringList ?
    // int m_arrayIndex;
};

SettingsPrivate::SettingsPrivate(const QString &organization, const QString &application)
    : m_client(0)
    , m_organizationName(organization)
    , m_applicationName(application)
{
    m_prefix = (QString("/org/") + m_organizationName + "/").toLatin1();
    if (!m_applicationName.isEmpty())
    {
        m_prefix += (m_applicationName + "/").toLatin1();
    }

// not sure if this condition should be compile-time:
#if (G_ENCODE_VERSION (GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION)) < GLIB_VERSION_2_36
    g_type_init();
#endif
    m_client = dconf_client_new();
}

SettingsPrivate::~SettingsPrivate()
{
    if (m_client)
    {
        g_object_unref(m_client);
    }
}

void SettingsPrivate::clear()
{

}

void SettingsPrivate::sync()
{
    dconf_client_sync(m_client);
}

QSettings::Status SettingsPrivate::status() const
{
    return QSettings::NoError; // FIXME: should we omit errors?
}

// FIXME: what if prefix contains '/' ?
void SettingsPrivate::beginGroup(const QString &prefix)
{
    m_prefix.append(prefix + '/');

    qDebug("beginGroup, current prefix: %s", m_prefix.constData());
}

void SettingsPrivate::endGroup()
{
    if (m_prefix.length() > 2)
    {
        int sep = m_prefix.lastIndexOf('/', m_prefix.length() - 2);
        if (sep != -1)
        {
            m_prefix = m_prefix.left(sep + 1);
        }
        qDebug("endGroup, current prefix: %s", m_prefix.constData());
    }
}

QString SettingsPrivate::group() const
{
    if (m_prefix.length() > 2)
    {
        int sep = m_prefix.lastIndexOf('/', m_prefix.length() - 2);
        if (sep != -1)
        {
            ++sep;
            return m_prefix.mid(sep, m_prefix.length() - sep - 1);
        }
    }
    return QString();
}

int SettingsPrivate::beginReadArray(const QString &prefix)
{
    return 0;
}

void SettingsPrivate::beginWriteArray(const QString &prefix, int size)
{
}

void SettingsPrivate::endArray()
{
}

void SettingsPrivate::setArrayIndex(int i)
{
}

QStringList SettingsPrivate::allKeys() const
{
    QStringList result;

    // TODO: implement

    return result;
}

QStringList SettingsPrivate::childKeys() const
{
    QStringList result;
    int len;
    char **keys = dconf_client_list(m_client, m_prefix.constData(), &len);

    if (keys)
    {
        for (char **key = keys; *key; ++key)
        {
            if (!dconf_is_key(*key, NULL)) // if this is not a dir
            {
                result.append(*key);
            }
        }

        g_strfreev(keys);
    }

    return result;
}

QStringList SettingsPrivate::childGroups() const
{
    QStringList result;
    int len;
    char **keys = dconf_client_list(m_client, m_prefix.constData(), &len);

    if (keys)
    {
        for (char **key = keys; *key; ++key)
        {
            if (dconf_is_dir(*key, NULL))
            {
                result.append(*key);
            }
        }

        g_strfreev(keys);
    }

    return result;
}

bool SettingsPrivate::isWritable() const
{
    return bool(dconf_client_is_writable(m_client, m_prefix.constData()));
}

void SettingsPrivate::setValue(const QString &key, const QVariant &value)
{
    GError *err = NULL;
    QString str = variantToString(value);
    QByteArray path = m_prefix + key.toLatin1();
    GVariant *val = g_variant_new_string(str.toUtf8().constData());
    g_variant_ref_sink(val);
    qDebug("setValue: path: %s, value: %s", path.constData(), g_variant_get_string(val, NULL));
    dconf_client_write_fast(m_client, path.constData(), val, &err);
    if (val)
    {
        g_variant_unref(val);
    }

    if (err)
    {
        qDebug("error: %s", err->message);
        g_error_free(err);
    }
}

QVariant SettingsPrivate::value(const QString &key, const QVariant &defaultValue) const
{
    QByteArray path = m_prefix + key.toLatin1();
    qDebug("value(), path: %s", path.constData());
    GVariant *val = dconf_client_read(m_client, path.constData());
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
    QByteArray path = m_prefix + key.toLatin1();
    dconf_client_write_sync(m_client, path.constData(), NULL, NULL, NULL, NULL);
}

bool SettingsPrivate::contains(const QString &key) const
{
    return false;
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
            ? QCoreApplication::organizationName()
            : QCoreApplication::organizationDomain() ,
#else
        QCoreApplication::organizationName().isEmpty()
            ? QCoreApplication::organizationDomain()
            : QCoreApplication::organizationName() ,
#endif
        QCoreApplication::applicationName()))
{
}

Settings::Settings(const QString &organization, const QString &application, QObject *parent)
    : QObject(parent)
    , d_ptr(new SettingsPrivate(organization, application))
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

void Settings::beginWriteArray(const QString& prefix, int size)
{
    Q_D(Settings);
    return d->beginWriteArray(prefix, size);
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
