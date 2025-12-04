#pragma once 

// SPDX-FileCopyrightText: Inria <tristan.cabel@inria.fr>

#include "parameterTraits.h"

class ParameterBool : public ParameterBase<bool>
{
public:
    ParameterBool(void);
    ParameterBool(bool value);
    ParameterBool(const std::string& name, const std::string& description, bool value);
    ParameterBool(const ParameterBool& param);
    ~ParameterBool(void);

    std::string type() const override { return "bool"; }
};