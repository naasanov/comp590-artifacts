// SPDX-FileCopyrightText: Inria <tristan.cabel@inria.fr>

#include "parameterBool.h"

ParameterBool::ParameterBool(void) : ParameterBase<bool>()
{
}

ParameterBool::ParameterBool(bool value) : ParameterBase<bool>(value)
{
}

ParameterBool::ParameterBool(const std::string& name, const std::string& description, bool value) : ParameterBase<bool>(name, description, value)
{
}

ParameterBool::ParameterBool(const ParameterBool& param) : ParameterBase<bool>(param)
{
}

ParameterBool::~ParameterBool(void)
{
}
