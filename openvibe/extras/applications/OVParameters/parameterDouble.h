#pragma once 

// SPDX-FileCopyrightText: Inria <tristan.cabel@inria.fr>

#include "parameterTraits.h"
#include <float.h>

class ParameterDouble : public ParameterBase<double>
{
public:
    ParameterDouble(void);
    ParameterDouble(double value);
    ParameterDouble(const std::string& name, const std::string& description, double value);
    ParameterDouble(const std::string& name, const std::string& description, double value, double min, double max);
    ParameterDouble(const ParameterDouble& param);
    ~ParameterDouble(void);

    void setValue(const double& value, bool notify=true) override;

    double min() const;
    void setMin(double value, bool notify=true);
    void setMinNotifier(std::function<void(const double&)> func);

    double max() const;
    void setMax(double value, bool notify=true);
    void setMaxNotifier(std::function<void(const double&)> func);

    std::string type() const override { return "double"; }

protected:
    double m_min = DBL_MIN;
    std::function<void(const double&)> m_minNotifier = nullptr;
    double m_max = DBL_MAX;
    std::function<void(const double&)> m_maxNotifier = nullptr;
};

