#pragma once 

// SPDX-FileCopyrightText: Inria <tristan.cabel@inria.fr>

#include "parameterTraits.h"

class ParameterListStringObject;

class ParameterListString : public ParameterBase<std::string>
{
public:
    ParameterListString(void);
    ParameterListString(std::vector<std::string> available_values);
    ParameterListString(const std::string& name, const std::string& description, const std::vector<std::string>& available_values, const std::string& value = "");
    ParameterListString(const ParameterListString& param);
    ~ParameterListString(void);

    void setValue(const std::string& value, bool notify=true) override;

    std::vector<std::string> availableValues() const;
    void setAvailableValues(const std::vector<std::string>& values, bool notify=true);
    void setAvailableValuesNotifier(std::function<void(const std::vector<std::string>&)> func);

    std::string type() const override { return "stringlist"; }

protected:
    std::vector<std::string> m_available_values;
    std::function<void(const std::vector<std::string>&)> m_availableValuesNotifier = nullptr;
};