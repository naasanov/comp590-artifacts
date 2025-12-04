#pragma once 

// SPDX-FileCopyrightText: Inria <tristan.cabel@inria.fr>

#include "parameterObject.h"
#include "parameterBool.h"

class ParameterBoolObject: public ParameterObject
{
    Q_OBJECT
    Q_PROPERTY(bool value READ value WRITE setValue NOTIFY valueChanged)

public:
    ParameterBoolObject(const std::string& name, const std::string& description, bool value);
    ParameterBoolObject(ParameterTraits *param);
    virtual ~ParameterBoolObject();

    bool value() const;
    void setValue(bool value);

signals:
    void valueChanged(const bool&);

private:
    ParameterBool *m_bool = nullptr;
};

DECLARE_PARAMETER_REGISTRATION(ParameterBoolObject)