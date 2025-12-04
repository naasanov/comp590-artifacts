#pragma once 

// SPDX-FileCopyrightText: Inria <tristan.cabel@inria.fr>

#include "parameterObject.h"
#include "parameterListString.h"

class ParameterListStringObject: public ParameterObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList available_values READ availableValues WRITE setAvailableValues NOTIFY availableValuesChanged)
    Q_PROPERTY(QString value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(int index READ index WRITE setIndex NOTIFY indexChanged)


public:
    ParameterListStringObject(std::vector<std::string> available_values);
    ParameterListStringObject(const std::string& name, const std::string& description, const std::vector<std::string>& available_values, const std::string& value = "");
    ParameterListStringObject(ParameterTraits *param);
    virtual ~ParameterListStringObject();

    QStringList availableValues() const;
    void setAvailableValues(QStringList value);

    QString value() const;
    void setValue(QString value);

    int index() const;
    void setIndex(int index);

signals:
    void indexChanged(int);
    void valueChanged(const QString&);
    void availableValuesChanged(const QStringList&);

private:
    ParameterListString *m_list_string = nullptr;
};

DECLARE_PARAMETER_REGISTRATION(ParameterListStringObject)