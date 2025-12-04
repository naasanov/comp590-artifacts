#pragma once 

// SPDX-FileCopyrightText: Inria <tristan.cabel@inria.fr>

#include "parameterTraits.h"

class ParameterString : public ParameterBase<std::string>
{
public:
    ParameterString(void);
    ParameterString(const std::string& value);
    ParameterString(const std::string& name, const std::string& description, std::string value);
    ParameterString(const ParameterString& param);
    ~ParameterString(void);

    std::string type() const override { return "string"; }
};