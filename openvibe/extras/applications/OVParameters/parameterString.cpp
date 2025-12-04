// SPDX-FileCopyrightText: Inria <tristan.cabel@inria.fr>

#include "parameterString.h"

ParameterString::ParameterString(void) : ParameterBase<std::string>()
{
}

ParameterString::ParameterString(const std::string& value) : ParameterBase<std::string>(value)
{
}

ParameterString::ParameterString(const std::string& name, const std::string& description, std::string value) : ParameterBase<std::string>(name, description, value)
{
}

ParameterString::ParameterString(const ParameterString& param) : ParameterBase<std::string>(param)
{
}

ParameterString::~ParameterString(void)
{
}