// SPDX-FileCopyrightText: Inria <tristan.cabel@inria.fr>

#include "parameterObject.h"

ParameterObject::ParameterObject(ParameterTraits *param) 
{
    m_param = param;
}

ParameterObject::~ParameterObject() 
{
    //no delete here as it will be deleted by its children
    m_param = nullptr; 
}

QString ParameterObject::name() const 
{ 
    if(m_param) 
        return QString::fromStdString(m_param->name());
    return "";
}

QString ParameterObject::description() const 
{
    if(m_param)
        return QString::fromStdString(m_param->description());
    return "";
}

QString ParameterObject::type() const 
{
    qDebug() << Q_FUNC_INFO << " - Getting type: " << QString::fromStdString(m_param->type());
    if(m_param)
        return QString::fromStdString(m_param->type()); 
    return "";
}

void ParameterObject::setName(const QString &name) 
{
    if(m_param) {
        m_param->setName(name.toStdString());
        emit nameChanged(name);
    }
}

void ParameterObject::setDescription(const QString &description) 
{
    if(m_param) {
        m_param->setDescription(description.toStdString());
        emit descriptionChanged(description);
    }
}

ParameterTraits *ParameterObject::parameter() const 
{
    return m_param;
}

void ParameterObject::notifyNameChanged(const QString& n) 
{
    emit nameChanged(n);
}

void ParameterObject::notifyDescriptionChanged(const QString& d) 
{
    emit descriptionChanged(d);
}

ParameterObject *ParameterObject::fromParameter(ParameterTraits *param) {
    qDebug() << Q_FUNC_INFO << " - Creating parameter object from parameter: " 
             << QString::fromStdString(param->name())
             << "  type:  "<< QString::fromStdString(param->type());
    qDebug() << "keys " << OVParameters::object_factory().keys();
    auto func = OVParameters::object_factory()[param->type()];
    if(func)
        return func(param);
    return nullptr;
}

namespace OVParameters {
    QMap<std::string, std::function<ParameterObject*(ParameterTraits *)> > &object_factory() {
        static QMap<std::string, std::function<ParameterObject*(ParameterTraits *)> > _instance;
        return _instance;
    }
}