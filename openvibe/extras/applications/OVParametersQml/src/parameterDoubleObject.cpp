// SPDX-FileCopyrightText: Inria <tristan.cabel@inria.fr>

#include "parameterDoubleObject.h"

ParameterDoubleObject::ParameterDoubleObject(const std::string& name, const std::string& description, double value) : ParameterObject()
{
    m_double = new ParameterDouble(name, description, value);
    m_param = m_double;
    m_double->connect([this](std::any value) {
        emit this->valueChanged(std::any_cast<double>(value));
    });
    m_double->setMinNotifier([this](const double& value) {
        emit this->minChanged(value);
    });
    m_double->setMaxNotifier([this](const double& value) {
        emit this->maxChanged(value);
    });
}

ParameterDoubleObject::ParameterDoubleObject(const std::string& name, const std::string& description, double value, double min, double max) : ParameterObject()
{
    m_double = new ParameterDouble(name, description, value, min, max);
    m_param = m_double;
    m_double->connect([this](std::any value) {
        emit this->valueChanged(std::any_cast<double>(value));
    });
    m_double->setMinNotifier([this](const double& value) {
        emit this->minChanged(value);
    });
    m_double->setMaxNotifier([this](const double& value) {
        emit this->maxChanged(value);
    });
}

ParameterDoubleObject::ParameterDoubleObject(ParameterTraits *param) : ParameterObject(param)
{
    m_delete_param = false;
    m_double = dynamic_cast<ParameterDouble *>(param);
    m_double->connect([this](std::any value) {
        emit this->valueChanged(std::any_cast<double>(value));
    });
    m_double->setMinNotifier([this](const double& value) {
        emit this->minChanged(value);
    });
    m_double->setMaxNotifier([this](const double& value) {
        emit this->maxChanged(value);
    });
}

ParameterDoubleObject::~ParameterDoubleObject() 
{
    if(m_delete_param)
        delete m_double;
    m_double = nullptr;
}

double ParameterDoubleObject::value() const 
{
    return m_double->value();
}

void ParameterDoubleObject::setValue(double value) 
{
    qDebug() << Q_FUNC_INFO << " - Setting value: " << value;
    m_double->setValue(value, false);
}

double ParameterDoubleObject::min() const 
{
    return m_double->min();
}

void ParameterDoubleObject::setMin(double value) 
{
    m_double->setMin(value, false);
}

double ParameterDoubleObject::max() const 
{
    return m_double->max();
    return DBL_MAX;
}

void ParameterDoubleObject::setMax(double value) 
{
    m_double->setMax(value, false);
}

REGISTER_PARAMETER_OBJECT("double", ParameterDoubleObject)