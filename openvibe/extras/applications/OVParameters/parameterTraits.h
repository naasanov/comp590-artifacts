#pragma once 

// SPDX-FileCopyrightText: Inria <tristan.cabel@inria.fr>

#include <any>
#include <functional>
#include <iostream>
#include <list>
#include <memory>
#include <string>
#include <vector>

class ParameterTraits
{
public:
    ParameterTraits();
    ParameterTraits(const std::string& name, const std::string& description);
    ParameterTraits(const ParameterTraits& param);
    virtual ~ParameterTraits();

    std::string name() const;
    void setName(const std::string& name);

    std::string description() const;
    void setDescription(const std::string& description);

    virtual void setValue(const std::any& value, bool notify=true) = 0;
    virtual std::any valueAsAny() const = 0;
    virtual std::string type() const = 0;
 
public:
    void sync(void); 
    int connect(std::function<void(std::any)> slot);
    void disconnect(int idx);
    void disconnect(void);

protected:    
    std::string m_name;
    std::string m_description;
    std::list<std::function<void(std::any)> > m_connections;
};

template <typename T>
class ParameterBase: public ParameterTraits
{
public:
    ParameterBase() : ParameterTraits() {}

    ParameterBase(const std::string& name, const std::string& description) : ParameterTraits(name, description) {}

    ParameterBase(const std::string& name, const std::string& description, T value) : ParameterTraits(name, description) {
        setValue(value);
    }

    ParameterBase(const T& value) : ParameterTraits() {
        setValue(value);
    }

    ParameterBase(const ParameterBase& param) : ParameterTraits(param) {
        m_value = param.m_value;
    }

    virtual ~ParameterBase() = default;

    virtual const T& value() const {
        return m_value;
    }

    auto operator=(const T& value) {
        setValue(value);
        return *this;
    }

    bool operator==(const T& value) const {
        return m_value == value;
    }
    
    virtual void setValue(const std::any& value, bool notify=true) override {
        if (value.type() == typeid(T)) {
            m_value = std::any_cast<T>(value);
            if (notify)
                sync();
        } else {
            std::cout << "ParameterBase::setValue: cannot convert value" 
                      << value.type().name() << " to type " << this->type() << std::endl;
        }
    }

    virtual std::any valueAsAny() const override {
        return std::any(m_value);
    }

    virtual void setValue(const T& value, bool notify=true) {
        m_value = value;
        if (notify)
            sync();
    }
    
protected:
    T m_value;
};

using Parameters = std::vector<ParameterTraits*>;