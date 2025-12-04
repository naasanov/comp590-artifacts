// SPDX-FileCopyrightText: Inria <tristan.cabel@inria.fr>

#include "parameterBoolObject.h"


ParameterBoolObject::ParameterBoolObject(const std::string& name, const std::string& description, bool value) : ParameterObject()
{
    m_bool = new ParameterBool(name, description, value);
    m_param = m_bool;
    m_bool->connect([this](std::any value) {
        emit this->valueChanged(std::any_cast<bool>(value));
    });
}
ParameterBoolObject::ParameterBoolObject(ParameterTraits *param) : ParameterObject(param)
{
    m_delete_param = false;
    m_bool = dynamic_cast<ParameterBool *>(param);
    m_bool->connect([this](std::any value) {
        emit this->valueChanged(std::any_cast<bool>(value));
    });
}

ParameterBoolObject::~ParameterBoolObject() 
{
    if(m_delete_param)
       delete m_bool;
    m_bool = nullptr;
}

bool ParameterBoolObject::value() const 
{
    return m_bool->value();
}

void ParameterBoolObject::setValue(bool value) 
{
    qDebug() << Q_FUNC_INFO << " - Setting value: " << value;
    m_bool->setValue(value, false);
}

REGISTER_PARAMETER_OBJECT("bool", ParameterBoolObject)