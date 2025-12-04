// SPDX-FileCopyrightText: Inria <tristan.cabel@inria.fr>

#include "parameterInt.h"

ParameterInt::ParameterInt(void) : ParameterBase<int>()
{
}

ParameterInt::ParameterInt(int value) : ParameterBase<int>(value)
{
}

ParameterInt::ParameterInt(const std::string& name, const std::string& description, int value) : ParameterBase<int>(name, description, value)
{
}

ParameterInt::ParameterInt(const std::string& name, const std::string& description, int value, int min, int max) : ParameterBase<int>(name, description, value)
{
    m_min = min;
    m_max = max;
}

ParameterInt::ParameterInt(const ParameterInt& param) : ParameterBase<int>(param)
{
    m_min = param.m_min;
    m_max = param.m_max;
}

ParameterInt::~ParameterInt(void)
{
}

void ParameterInt::setValue(const int& value, bool notify)
{
    if(value < m_min) {
        m_value = m_min;
        sync(); // forece sync here to notify the gui that it's a new value
    }
    else if(value > m_max) {
        m_value = m_max;
        sync();
    }
    else {
        m_value = value;
        if(notify)
            sync();
    }
}

int ParameterInt::min() const
{
    return m_min;
}

void ParameterInt::setMin(int value, bool notify)
{
    m_min = value;
    if(m_max < value) {
       m_max = value;
        if(m_maxNotifier) {
            m_maxNotifier(m_max);
        }
    }

    if(this->value() < value)
        this->setValue(value);

    if(notify && m_minNotifier) {
        m_minNotifier(m_min);
    }
}

void ParameterInt::setMinNotifier(std::function<void(const int&)> func)
{
    m_minNotifier = func;
}

int ParameterInt::max() const
{
    return m_max;
}

void ParameterInt::setMax(int value, bool notify)
{
    m_max = value;
    if(m_min > value) {
       m_min = value;
        if(m_minNotifier) {
            m_minNotifier(m_min);
        }
    }

    if(this->value() > value)
        this->setValue(value);

    if(notify && m_maxNotifier) {      
        m_maxNotifier(m_max);
    }
}

void ParameterInt::setMaxNotifier(std::function<void(const int&)> func)
{
    m_maxNotifier = func;
}