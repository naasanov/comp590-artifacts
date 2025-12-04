// SPDX-FileCopyrightText: Inria <tristan.cabel@inria.fr>

#include "parameterListStringObject.h"

ParameterListStringObject::ParameterListStringObject(std::vector<std::string> available_values): ParameterObject()
{
    m_list_string = new ParameterListString(available_values);
    m_param = m_list_string;
    m_list_string->connect([this](std::any value) {
        const QString ss = QString::fromStdString(std::any_cast<std::string>(value));
        emit this->valueChanged(ss);
    });
    m_list_string->setAvailableValuesNotifier([this](const std::vector<std::string>& values) {
        QStringList ll;
        for(auto s : values) {
            ll << QString::fromStdString(s);
        }
        emit this->availableValuesChanged(ll);
    });

    m_list_string->sync();
}

ParameterListStringObject::ParameterListStringObject(const std::string& name, const std::string& description, const std::vector<std::string>& available_values, const std::string& value) : ParameterObject()
{
    m_list_string = new ParameterListString(name, description, available_values, value);
    m_param = m_list_string;
    m_list_string->connect([this](std::any value) {
        const QString ss = QString::fromStdString(std::any_cast<std::string>(value));
        emit this->valueChanged(ss);
    });
    m_list_string->setAvailableValuesNotifier([this](const std::vector<std::string>& values) {
        QStringList ll;
        for(auto s : values) {
            ll << QString::fromStdString(s);
        }
        emit this->availableValuesChanged(ll);
    });

    m_list_string->sync();
}

ParameterListStringObject::ParameterListStringObject(ParameterTraits *param) : ParameterObject(param)
{
    m_delete_param = false;
    m_list_string = dynamic_cast<ParameterListString *>(param);
    m_param = m_list_string;
    m_list_string->connect([this](std::any value) {
        const QString ss = QString::fromStdString(std::any_cast<std::string>(value));
        emit this->valueChanged(ss);
    });
    m_list_string->setAvailableValuesNotifier([this](const std::vector<std::string>& values) {
        QStringList ll;
        for(auto s : values) {
            ll << QString::fromStdString(s);
        }
        emit this->availableValuesChanged(ll);
    });
}

ParameterListStringObject::~ParameterListStringObject() 
{
    if(m_delete_param)
        delete m_list_string;
    m_list_string = nullptr;
}

QString ParameterListStringObject::value() const 
{
    return QString::fromStdString(m_list_string->value());
}

void ParameterListStringObject::setValue(QString value) 
{
    qDebug() << Q_FUNC_INFO << " - Setting value: " << value;
    m_list_string->setValue(value.toStdString(), false);
}

QStringList ParameterListStringObject::availableValues() const
{
    QStringList ll;
    for(auto s : m_list_string->availableValues()) {
        ll << QString::fromStdString(s);
    }
    return ll;
}

void ParameterListStringObject::setAvailableValues(QStringList values) 
{
    qDebug() << Q_FUNC_INFO << " - Setting available values: " << values;
    std::vector<std::string> vv;
    for(auto s : values) {
        vv.push_back(s.toStdString());
    }
    m_list_string->setAvailableValues(vv, false);
}

int ParameterListStringObject::index() const 
{
    auto it = std::find(m_list_string->availableValues().begin(), m_list_string->availableValues().end(), m_list_string->value());
    if(it != m_list_string->availableValues().end()) {
        return std::distance(m_list_string->availableValues().begin(), it);
    }

    return -1;
}

void ParameterListStringObject::setIndex(int index) 
{
    m_list_string->setValue(m_list_string->availableValues().at(index), false);
}

REGISTER_PARAMETER_OBJECT("list_string", ParameterListStringObject)