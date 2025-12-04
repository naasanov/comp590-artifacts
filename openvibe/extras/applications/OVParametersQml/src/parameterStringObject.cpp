
// SPDX-FileCopyrightText: Inria <tristan.cabel@inria.fr>

#include "parameterStringObject.h"

ParameterStringObject::ParameterStringObject(const std::string& name, const std::string& description, const QString& value) : ParameterObject()
{
    m_string = new ParameterString(name, description, value.toStdString());
    m_param = m_string;
    m_string->connect([this](std::any value) {
        const QString ss = QString::fromStdString(std::any_cast<std::string>(value));
        emit this->valueChanged(ss);
    });
}

ParameterStringObject::ParameterStringObject(ParameterTraits *param) : ParameterObject(param)
{
    m_delete_param = false;
    m_string = dynamic_cast<ParameterString *>(param);
    m_param = m_string;
    m_string->connect([this](std::any value) {
        const QString ss = QString::fromStdString(std::any_cast<std::string>(value));
        emit this->valueChanged(ss);
    });

}

ParameterStringObject::~ParameterStringObject() 
{
    if(m_delete_param)
        delete m_string;
    m_string = nullptr;
}

QString ParameterStringObject::value() const 
{
    return QString::fromStdString(m_string->value());
}

void ParameterStringObject::setValue(QString value) 
{
    qDebug() << Q_FUNC_INFO << " - Setting value: " << value;
    m_string->setValue(value.toStdString(), false);
}

REGISTER_PARAMETER_OBJECT("string", ParameterStringObject)