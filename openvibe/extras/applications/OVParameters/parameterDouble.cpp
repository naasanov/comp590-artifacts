// SPDX-FileCopyrightText: Inria <tristan.cabel@inria.fr>

#include "parameterDouble.h"

ParameterDouble::ParameterDouble(void) : ParameterBase<double>()
{
}

ParameterDouble::ParameterDouble(double value) : ParameterBase<double>(value)
{
}

ParameterDouble::ParameterDouble(const std::string& name, const std::string& description, double value) : ParameterBase<double>(name, description, value)
{
}

ParameterDouble::ParameterDouble(const std::string& name, const std::string& description, double value, double min, double max) : ParameterBase<double>(name, description, value)
{
    m_min = min;
    m_max = max;
}

ParameterDouble::ParameterDouble(const ParameterDouble& param) : ParameterBase<double>(param)
{
    m_min = param.m_min;
    m_max = param.m_max;
}

ParameterDouble::~ParameterDouble(void)
{
}

void ParameterDouble::setValue(const double& value, bool notify)
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

double ParameterDouble::min() const
{
    return m_min;
}

void ParameterDouble::setMin(double value, bool notify)
{
    m_min = value;
    if(m_max < value) {
       m_max = value;
       if(m_maxNotifier)
           m_maxNotifier(m_max);
    }

    if(this->value() < value) {
        this->setValue(value);
        this->sync(); // let the gui know
    }

    if(notify && m_minNotifier) {
        m_minNotifier(m_min);
    }
}

void ParameterDouble::setMinNotifier(std::function<void(const double&)> func)
{
    m_minNotifier = func;
}

double ParameterDouble::max() const
{
    return m_max;
}

void ParameterDouble::setMax(double value, bool notify)
{
    m_max = value;
    if(m_min > value) {
        m_min = value;
        if(m_minNotifier)
            m_minNotifier(m_min);
    }

    if(this->value() > value) {
        this->setValue(value);
        this->sync(); // let the gui know
    }

    if(notify && m_maxNotifier) {
        m_maxNotifier(m_max);
    }
}

void ParameterDouble::setMaxNotifier(std::function<void(const double&)> func)
{
    m_maxNotifier = func;
}