// SPDX-FileCopyrightText: Inria <tristan.cabel@inria.fr>

#include <algorithm>
#include "parameterListString.h"

ParameterListString::ParameterListString(void) : ParameterBase<std::string>()
{
}

ParameterListString::ParameterListString(std::vector<std::string> available_values) : ParameterBase<std::string>()
{
    m_available_values = available_values;
    if(available_values.size() > 0) {
        m_value = available_values.at(0);
    }
}

ParameterListString::ParameterListString(const std::string& name, const std::string& description, const std::vector<std::string>& available_values, const std::string& value) : ParameterBase<std::string>(name, description, value)
{
    m_available_values = available_values;
    if(available_values.size() > 0) {
        m_value = available_values.at(0);
    }

    if(!value.empty())
        setValue(value);
}

ParameterListString::ParameterListString(const ParameterListString& param) : ParameterBase<std::string>(param)
{
    m_available_values = param.m_available_values;
}

ParameterListString::~ParameterListString(void)
{
}

void ParameterListString::setValue(const std::string& value, bool notify)
{
    if(std::find(m_available_values.begin(), m_available_values.end(), value) != m_available_values.end()) {
        m_value = value;
        if(notify)
            sync();
    } else {
        std::cout << "Parameter ListString " << this->name() << "cannot set value to " << value << std::endl;
    }
}

std::vector<std::string> ParameterListString::availableValues() const
{
    return m_available_values;
}

void ParameterListString::setAvailableValues(const std::vector<std::string>& values, bool notify)
{
    m_available_values = values;
    if(std::find(m_available_values.begin(), m_available_values.end(), m_value) == m_available_values.end()) {
        m_value = m_available_values.at(0);
        sync();
    }

    if(notify && m_availableValuesNotifier)
        m_availableValuesNotifier(m_available_values);
}

void ParameterListString::setAvailableValuesNotifier(std::function<void(const std::vector<std::string>&)> func)
{
    m_availableValuesNotifier = func;
}