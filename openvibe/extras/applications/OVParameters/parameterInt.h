#pragma once 

// SPDX-FileCopyrightText: Inria <tristan.cabel@inria.fr>

#include "parameterTraits.h"
#include <limits.h>

class ParameterIntObject;

class ParameterInt : public ParameterBase<int>
{
public:
    ParameterInt(void);
    ParameterInt(int value);
    ParameterInt(const std::string& name, const std::string& description, int value);
    ParameterInt(const std::string& name, const std::string& description, int value, int min, int max);
    ParameterInt(const ParameterInt& param);
    ~ParameterInt(void);

    void setValue(const int& value, bool notify=true) override;

    int min() const;
    void setMin(int value, bool notify=true);
    void setMinNotifier(std::function<void(const int&)> func);

    int max() const;
    void setMax(int value, bool notify=true);
    void setMaxNotifier(std::function<void(const int&)> func);

    std::string type() const override { return "int"; }

protected:
    int m_min = INT_MIN;
    std::function<void(const int&)> m_minNotifier = nullptr;
    int m_max = INT_MAX;
    std::function<void(const int&)> m_maxNotifier = nullptr;
};