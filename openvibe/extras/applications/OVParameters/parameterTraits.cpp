// SPDX-FileCopyrightText: Inria <tristan.cabel@inria.fr>

#include <iterator>
#include "parameterTraits.h"

ParameterTraits::ParameterTraits() {}

ParameterTraits::ParameterTraits(const std::string& name, const std::string& description)
    : m_name(name)
    , m_description(description)
{ }

ParameterTraits::ParameterTraits(const ParameterTraits& param)
{
    m_name = param.m_name;
    m_description = param.m_description;
    m_connections = param.m_connections;
}

ParameterTraits::~ParameterTraits()
{
}

int ParameterTraits::connect(std::function<void(std::any)> slot)
{
    m_connections.push_back(slot);
    return m_connections.size() - 1;
}    

void ParameterTraits::disconnect(int idx) {
    auto it = std::next(m_connections.begin(), idx);
    m_connections.erase(it);
}

void ParameterTraits::disconnect(void)
{
    m_connections.clear();
}

std::string ParameterTraits::name() const
{
    return m_name;
}

void ParameterTraits::setName(const std::string& name)
{
    m_name = name;
}

std::string ParameterTraits::description() const
{
    return m_description;
}

void ParameterTraits::setDescription(const std::string& description)
{
    m_description = description;
}

void ParameterTraits::sync(void) {
    for(auto& c : m_connections) {
        c(this->valueAsAny());
    }
}