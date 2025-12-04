#pragma once 

// SPDX-FileCopyrightText: Inria <tristan.cabel@inria.fr>

#include "parameterObject.h"
#include "parameterDouble.h"

class ParameterDoubleObject: public ParameterObject
{
    Q_OBJECT
    Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(double min READ min WRITE setMin NOTIFY minChanged)
    Q_PROPERTY(double max READ max WRITE setMax NOTIFY maxChanged)

public:
    ParameterDoubleObject(const std::string& name, const std::string& description, double value);
    ParameterDoubleObject(const std::string& name, const std::string& description, double value, double min, double max);
    ParameterDoubleObject(ParameterTraits *param);
    virtual ~ParameterDoubleObject();

    double value() const;
    void setValue(double value);

    double min() const;
    void setMin(double value);

    double max() const;
    void setMax(double value);

signals:
    void valueChanged(const double&);
    void minChanged(const double&);
    void maxChanged(const double&);

private:
    ParameterDouble *m_double = nullptr;
};

DECLARE_PARAMETER_REGISTRATION(ParameterDoubleObject)