// SPDX-FileCopyrightText: Inria <tristan.cabel@inria.fr>

#include "parameterIntObject.h"

ParameterIntObject::ParameterIntObject(const std::string& name, const std::string& description, int value): ParameterObject()
{
    m_int = new ParameterInt(name, description, value);
    m_param = m_int;
    m_int->connect([this](std::any value) {
            emit this->valueChanged(std::any_cast<int>(value));
    });
    m_int->setMinNotifier([this](const int& value) {
            emit this->minChanged(value);
    });
    m_int->setMaxNotifier([this](const int& value) {
            emit this->maxChanged(value);
    });
}

ParameterIntObject::ParameterIntObject(const std::string& name, const std::string& description, int value, int min, int max): ParameterObject()
{
    m_int = new ParameterInt(name, description, value, min, max);
    m_param = m_int;
    m_int->connect([this](std::any value) {
            emit this->valueChanged(std::any_cast<int>(value));
    });
    m_int->setMinNotifier([this](const int& value) {
            emit this->minChanged(value);
    });
    m_int->setMaxNotifier([this](const int& value) {
            emit this->maxChanged(value);
    });
}


ParameterIntObject::ParameterIntObject(ParameterTraits *param) : ParameterObject(param)
{
    m_delete_param = false;
    m_int = dynamic_cast<ParameterInt *>(param);
    m_int->connect([this](std::any value) {
            emit this->valueChanged(std::any_cast<int>(value));
    });
    m_int->setMinNotifier([this](const int& value) {
            emit this->minChanged(value);
    });
    m_int->setMaxNotifier([this](const int& value) {
            emit this->maxChanged(value);
    });
}

ParameterIntObject::~ParameterIntObject() 
{
    if(m_delete_param)
        delete m_int;
    m_int = nullptr;
}

int ParameterIntObject::value() const 
{
    return m_int->value();
}

void ParameterIntObject::setValue(int value) 
{
    m_int->setValue(value, false);
}

int ParameterIntObject::min() const
{
    return m_int->min();
}

void ParameterIntObject::setMin(int value)
{
    m_int->setMin(value, false);
}

int ParameterIntObject::max() const
{
    return m_int->max();
}

void ParameterIntObject::setMax(int value)
{
    m_int->setMax(value, false);
}
