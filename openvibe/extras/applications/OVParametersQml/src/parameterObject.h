#pragma once

// SPDX-FileCopyrightText: Inria <tristan.cabel@inria.fr>

#include <QtCore>
#include <parameterTraits.h>

class ParameterObject : public QObject 
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString description READ description WRITE setDescription NOTIFY descriptionChanged)
    Q_PROPERTY(QString type READ type CONSTANT)

public:
    ParameterObject(void) = default;
    ParameterObject(ParameterTraits *param);
    virtual ~ParameterObject();

    QString name() const;
    QString description() const;
    QString type() const;
    void setName(const QString &name);
    void setDescription(const QString &description);
    ParameterTraits *parameter() const;
    void notifyNameChanged(const QString& n);
    void notifyDescriptionChanged(const QString& d);

public:
    static ParameterObject *fromParameter(ParameterTraits *param);

signals:
    void nameChanged(const QString&);
    void descriptionChanged(const QString&);    

public:
    ParameterTraits *m_param = nullptr;
    bool m_delete_param = true;
};

namespace OVParameters {
    QMap<std::string, std::function<ParameterObject*(ParameterTraits *)> > &object_factory();
}

#define DECLARE_PARAMETER_REGISTRATION(class_name) \
 namespace class_name##Dummy { \
    bool class_name##Registration(); \
    extern const bool class_name##_registered; \
    }

#define REGISTER_PARAMETER_OBJECT(type, class_name) \
  namespace class_name##Dummy { \
    bool class_name##Registration() { \
        std::function<ParameterObject*(ParameterTraits *)> create_##class_name = [](ParameterTraits *p) { return new class_name(p); }; \
        OVParameters::object_factory().insert(type, create_##class_name); \
        return true; \
    } \
    const bool class_name##_registered = class_name##Registration(); \
  }

using ParameterObjects = std::vector<ParameterObject*>;