/*
    Copyright 2013 Saša Vilić

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Lesser License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Lesser Licence for more details.

    You should have received a copy of the GNU General Lesser License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef LIBCONFIGPP_H
#define LIBCONFIGPP_H

#include <stdexcept>
#include <map>
#include <vector>
#include <algorithm>
#include <string>
#include <iostream>
#include <sstream>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/static_assert.hpp>
#include <boost/tokenizer.hpp>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>
#include <boost/any.hpp>

namespace libconfig {

class ConfigException : public std::runtime_error
{
public:
    ConfigException(const std::string& error) : std::runtime_error(error) {}
    virtual ~ConfigException() throw() {}
};

class SettingException : public ConfigException
{
public:
    SettingException(const std::string& error, const std::string& path)
        : ConfigException(error),
          m_path(path)
    {}

    const char* getPath() const
    {
        return m_path.c_str();
    }

    std::string path() const
    {
        return m_path;
    }

    virtual ~SettingException() throw() {}
private:
    std::string m_path;
};

class SettingNotFoundException : public SettingException
{
public:
    SettingNotFoundException(const std::string& error, const std::string& path)
        : SettingException(error, path)
    {}

    virtual ~SettingNotFoundException() throw() {}
};

class SettingNameException : public SettingException
{
public:
    SettingNameException(const std::string& error, const std::string& path)
        : SettingException(error, path)
    {}

    virtual ~SettingNameException() throw() {}
};

class SettingTypeException : public SettingException
{
public:
    SettingTypeException(const std::string& error, const std::string& path)
        : SettingException(error, path)
    {}

    virtual ~SettingTypeException() throw() {}
};

class FileIOException : public ConfigException
{
public:
    FileIOException(const std::string& error)
        : ConfigException(error)
    {}

    virtual ~FileIOException() throw() {}
};

class ParseException : public ConfigException
{
public:
    ParseException(const std::string& error, const std::string& file,
                   size_t line, size_t offset)
        : ConfigException(error),
          m_file(file),
          m_line(line),
          m_offset(offset)
    {}

    virtual ~ParseException() throw() {}

    const char * getError() const
    {
        return what();
    }

    const char* getFile() const
    {
        return m_file.c_str();
    }

    const int getLine() const
    {
        return m_line;
    }

    const std::string& file() const
    { return m_file; }

    size_t line() const
    { return m_line; }

    size_t offset() const
    { return m_offset;}

private:
    std::string m_file;
    size_t m_line;
    size_t m_offset;
};

template<typename charT>
class basic_setting
{
public:
    typedef charT char_type;
    typedef typename std::basic_string<charT> string_type;

    enum Type {
        TypeInt,
        TypeInt64,
        TypeFloat,
        TypeString,
        TypeBoolean,
        TypeArray,
        TypeList,
        TypeGroup
    };

    explicit basic_setting(const string_type& name)
        : m_name(name),
          m_type(TypeGroup),
          m_parent(0),
          m_value(new _basic_setting_container(this))
    {
    }

    template<typename T>
    basic_setting(const string_type& name, const T& value)
        : m_name(name),
          m_type(_deduce_scalar_type(value)),
          m_parent(0),
          m_value(new _basic_setting_scalar<T>(value))
    {
    }

    basic_setting(const string_type& name, const basic_setting& value)
        : m_name(name),
          m_type(TypeGroup),
          m_parent(0),
          m_value(new _basic_setting_container(this))
    {
        m_value->add(value);
    }

    template<typename T>
    basic_setting(const string_type &name, const std::vector<T>& values)
        : m_name(name),
          m_type(_deduce_scalar_type<T>(0) != TypeArray ? TypeArray : TypeArray ),
          m_parent(0),
          m_value(new _basic_setting_list(this))
    {
        for(typename std::vector<T>::const_iterator it = values.begin(); it != values.end(); ++it) {
            m_value->add(basic_setting(*it));
        }
    }

    basic_setting(const string_type &name, const std::vector<basic_setting>& values)
        : m_name(name),
          m_type(TypeList),
          m_parent(0),
          m_value(new _basic_setting_list(this, values))
    {
    }

    basic_setting(const basic_setting& other)
        : m_name(other.m_name),
          m_type(other.m_type),
          m_parent(0),
          m_value(other.m_value->clone(this))
    {
    }

    basic_setting& operator =(const basic_setting& other)
    {
        if (this != &other) {
            m_name = other.m_name;
            m_type = other.m_type;
            m_value.reset(other.m_value->clone(this));
        }
        return *this;
    }

    bool operator==(const basic_setting& other) const
    {
        if (m_name == other.m_name && m_type == other.m_type) {
            if (m_value && other.m_value) {
                return *m_value == *other.m_value;
            } else if (!m_value && !other.m_value) {
                return true;
            }
            return true;
        }
        return false;
    }

    virtual ~basic_setting() {}

    string_type name() const
    { return m_name; }

    const char* getName() const
    { return m_name.c_str(); }

    template <typename T>
    operator T () const
    {
        T result;
        m_value->lookupValue(result);
        return result;
    }

    template<typename T>
    basic_setting& operator=(const T& value)
    {
        m_type = _deduce_scalar_type(value);
        if (isScalar()) {
            m_value.reset(new _basic_setting_scalar<T>(value));
        }
        return *this;
    }

    template<typename T>
    basic_setting& operator[](const T index)
    {
        return _at(index);
    }

    template<typename T>
    const basic_setting& operator[](const T index) const
    {
        return _at(index);
    }

    basic_setting& operator[](int index)
    {
        _check_index(index);
        return m_value->at(index);
    }

    const basic_setting& operator[](int index) const
    {
        _check_index(index);
        return m_value->at(index);
    }

    template <typename T>
    bool lookupValue(const string_type& path, T& value) const
    {
        try {
            const basic_setting& s = _at(path);
            value = s;
            return true;
        } catch (std::exception&) {
            return false;
        }
    }

    template<typename T>
    basic_setting& add(const string_type& name, const T& value)
    {
        basic_setting setting(name, value);
        m_value->add(setting);
        return setting;
    }

    basic_setting& add(const basic_setting& setting)
    {
        return m_value->add(setting);
    }

    void remove(const string_type& path)
    {
        _check_path(path);
        _at(_parent(path))->remove(_leaf(path));
    }

    void remove(size_t position)
    {
        m_value->remove(position);
    }

    string_type& path() const
    {
        string_type path;
        if(m_parent) {
            path += m_parent->path();
        }

        if (path.size() > 0) {
            path += '.';
        }

        path += m_name;
        return path;
    }

    string_type getPath() const
    {
        string_type path;
        if(m_parent) {
            path += m_parent->path();
        }

        if (path.size() > 0) {
            path += '.';
        }

        path += m_name;
        return path;
    }

    const basic_setting& parent() const
    {
        return *m_parent;
    }

    basic_setting& parent()
    {
        return *m_parent;
    }

    const basic_setting& getParent() const
    {
        return *m_parent;
    }

    basic_setting& getParent()
    {
        return *m_parent;
    }

    int index() const
    {
        if(m_parent) {
            return m_parent->indexOf(*this);
        }
        return -1;
    }

    int getIndex() const
    {
        if(m_parent) {
            return m_parent->indexOf(*this);
        }
        return -1;
    }

    Type type() const
    {
        return m_type;
    }

    Type getType() const
    {
        return m_type;
    }

    bool exists(const string_type& path) const
    {
        _check_path(path);
        return _exists(path);
    }

    size_t getLength() const
    {
        return m_value->size();
    }

    size_t size() const
    {
        return m_value->size();
    }

    bool isGroup() const
    {
        return m_type == TypeGroup;
    }

    bool isArray() const
    {
        return m_type == TypeArray;
    }

    bool isList() const
    {
        return m_type == TypeList;
    }

    bool isAggredate() const
    {
        return isGroup() || isArray() || isList();
    }

    bool isScalar() const
    {
        return isNumber() ||
                m_type == TypeBoolean ||
                m_type == TypeString;
    }

    bool isNumber() const
    {
        switch (m_type) {
        case TypeInt:
        case TypeInt64:
        case TypeFloat:
            return true;
        default:
            return false;
        }
    }

    bool isRoot() const
    {
        return !m_parent;
    }

    const char* getSourceFile() const
    {
        return m_file.c_str();
    }

    const string_type& source_file() const
    {
        return m_file;
    }

    int getSourceLine() const
    {
        return m_line;
    }

    size_t source_line() const
    {
        return m_line;
    }

    template<typename T>
    friend std::ostream& operator<<(std::ostream &o, const basic_setting<T>& rhs);
protected:
    basic_setting(const string_type &name, const std::vector<basic_setting>& values, Type type)
        : m_name(name),
          m_type(type),
          m_parent(0),
          m_value(new _basic_setting_list(this, values))
    {
        BOOST_ASSERT(type == TypeList || type == TypeArray);
    }

    string_type m_file;
    size_t m_line;

private:
    string_type _local(const string_type& path) const
    {
        if (_long_path(path)) {
            return path.substr(0, path.find_first_of('.'));
        }
        return path;
    }

    string_type _remote(const string_type& path) const
    {
        if (_long_path(path)) {
            return path.substr(path.find_first_of('.')+1);
        }
        return "";
    }

    string_type _parent(const string_type& path) const
    {
        if(_long_path(path)) {
            return path.substr(path.substr(0, path.find_last_of('.')));
        }
        return "";
    }

    string_type _leaf(const string_type& path) const
    {
        if(_long_path(path)) {
            return path.substr(path.substr(path.find_last_of('.')+1));
        }
        return path;
    }

    bool _long_path(const string_type& path) const
    {
        return path.find_first_of('.') != string_type::npos;
    }

    bool _exists(const string_type& path) const
    {
        size_t index = 0;
        if(!_long_path(path)) {
            if(_convert_index(path, &index)) {
                return m_value->exists(index);
            } else {
                return m_value->exists(path);
            }
        } else {
            string_type local = _local(path);
            string_type remote = _remote(path);
            if(_convert_index(path, &index)) {
                if(m_value->exists(index)) {
                    return m_value->at(index)->_exists(remote);
                }
            } else {
                if (m_value->exists(local)) {
                    return m_value->at(local)->_exists(remote);
                }
            }
        }
        return false;
    }

    basic_setting& _at(const string_type& path)
    {
        if(path.empty()) {
            return *this;
        }

        try {
            size_t index = 0;
            if(!_long_path(path)) {
                if(_convert_index(path, &index)) {
                    return m_value->at(index);
                } else {
                    return m_value->at(path);
                }
            } else {
                string_type local = _local(path);
                string_type remote = _remote(path);
                if(_convert_index(local, &index)) {
                    return m_value->at(index)._at(remote);
                } else {
                    return m_value->at(local)._at(remote);
                }
            }
        } catch (SettingNotFoundException &ex) {
            throw _not_found_ex(ex, path);
        }
    }

    const basic_setting& _at(const string_type& path) const
    {
        if(path.empty()) {
            return *this;
        }

        try {
            size_t index = 0;
            if(!_long_path(path)) {
                if(_convert_index(path, &index)) {
                    return m_value->at(index);
                } else {
                    return m_value->at(path);
                }
            } else {
                string_type local = _local(path);
                string_type remote = _remote(path);
                if(_convert_index(local, index)) {
                    return m_value->at(index)._at(remote);
                } else {
                    return m_value->at(local)._at(remote);
                }
            }
        } catch (SettingNotFoundException &ex) {
            throw _not_found_ex(ex, path);
        }
    }

    bool _convert_index(const string_type& path, size_t *index) const
    {
        boost::regex rx_index("^\\[\\d+\\]\\.?");
        if (boost::regex_match(path, rx_index)) {
            std::basic_istringstream<char_type> iss(path.substr(1));
            size_t i;
            if (iss >> i) {
                *index = i;
                return true;
            }
        }
        return false;
    }

    void print(std::ostream& o, size_t level) const
    {
        if (!m_name.empty()) {
            o << m_name << " = ";
        }
        m_value->print(o, level);
    }

    template <typename T>
    static typename basic_setting<charT>::Type _deduce_scalar_type(const T&) {
        BOOST_STATIC_ASSERT(sizeof(T) == 0);
    }

    static Type _deduce_scalar_type(const bool&) {
        return TypeBoolean;
    }

    static Type _deduce_scalar_type(const int&) {
        return TypeInt;
    }

    static Type _deduce_scalar_type(const long&) {
        return TypeInt64;
    }

    static Type _deduce_scalar_type(const float&) {
        return TypeFloat;
    }

    static Type _deduce_scalar_type(const string_type&) {
        return TypeString;
    }

    int indexOf(const basic_setting& child) const
    {
        if (m_value) {
            return m_value->indexOf(child);
        }
        return -1;
    }

    class _basic_setting
    {
    public:
        virtual _basic_setting* clone(basic_setting* new_container) = 0;

        virtual bool operator==(const _basic_setting& other) const = 0;

        virtual void print(std::ostream&, size_t level) const = 0;

        virtual void lookupValue(bool&) {
            throw _type_ex("converion not implemented");
        }

        virtual void lookupValue(int&) {
            throw _type_ex("converion not implemented");
        }

        virtual void lookupValue(unsigned int&) {
            throw _type_ex("converion not implemented");
        }

        virtual void lookupValue(long&) {
            throw _type_ex("converion not implemented");
        }

        virtual void lookupValue(unsigned long&) {
            throw _type_ex("converion not implemented");
        }

        virtual void lookupValue(float&) {
            throw _type_ex("converion not implemented");
        }

        virtual void lookupValue(double&) {
            throw _type_ex("converion not implemented");
        }

        virtual void lookupValue(string_type&) {
            throw _type_ex("converion not implemented");
        }

        virtual basic_setting& at(const string_type& property)
        {
            throw _not_found_ex(property);
        }

        virtual basic_setting& at(size_t index)
        {
            throw _not_found_ex(index);
        }

        virtual bool exists(size_t index) const
        {
            return false;
        }

        virtual bool exists(const string_type &path) const
        {
            return false;
        }

        virtual basic_setting& add(const basic_setting&)
        {
            throw ConfigException("operation not supported");
        }

        virtual void remove(const string_type& property)
        {
            throw _not_found_ex(property);
        }

        virtual void remove(size_t index)
        {
            throw _not_found_ex(index);
        }

        virtual int indexOf(const basic_setting &child) const
        {
            return -1;
        }

        virtual size_t size() const
        {
            return 0;
        }
    };

    class _basic_setting_list : public _basic_setting
    {
        _basic_setting_list(const _basic_setting_list&) {}
        _basic_setting_list& operator=(const _basic_setting_list& other)
        { return *this; }

    public:
        typedef basic_setting<char_type> value_type;
        typedef boost::shared_ptr<value_type> value_ptr;

        _basic_setting_list(basic_setting* container,
                            const std::vector<value_type>& values = std::vector<value_type>())
            : m_container(container),
              rx_index("$\\[\\d+\\](\\..+)?")
        {
            for(size_t i=0; i<values.size(); i++) {
                value_ptr value(new value_type(values[i]));
                value->m_parent = container;
                m_properties.push_back(value);
            }
        }

        _basic_setting* clone(basic_setting *new_container)
        {
            _basic_setting_list* item = new _basic_setting_list(new_container);
            for(size_t i=0; i<m_properties.size(); i++) {
                item->add(*m_properties[i]);
            }
            return item;
        }

        bool operator==(const _basic_setting& other) const
        {
            const _basic_setting_list& o = static_cast<const _basic_setting_list&>(other);

            if(m_properties.size() == o.m_properties.size()) {
                for(size_t i=0; i<m_properties.size(); i++) {
                    if (!(*m_properties[i] == *o.m_properties[i])) {
                        return false;
                    }
                }
                return true;
            }
            return false;
        }

        void print(std::ostream& o, size_t level) const
        {
            if (m_container->m_type == TypeList) {
                print_list(o, level);
            } else {
                print_array(o);
            }
        }

        void print_list(std::ostream& o, size_t level) const
        {
            string_type ident_p(level * 4, ' ');
            string_type ident_c((level+1) * 4, ' ');
            o << "(\n";
            for(size_t i=0; i<m_properties.size(); i++) {
                if (i > 0)
                    o << ident_c << ",\n";
                o << ident_c;
                m_properties[i]->print(o, level+1);
                o << "\n";
            }
            o << ident_p << ")";
        }

        void print_array(std::ostream& o) const
        {
            o << "[";
            for(size_t i = 0; i<m_properties.size(); i++) {
                if (i>0)
                    o  << ", ";
                m_properties[i]->print(o, 0);
            }
            o << "]";
        }

        basic_setting& at(const string_type& path)
        {
            throw _not_found_ex(path);
        }

        basic_setting& at(size_t index)
        {
            if(index >= m_properties.size()) {
                throw _not_found_ex(index);
            }
            return *m_properties[index];
        }

        bool exists(size_t index) const
        {
            return index < m_properties.size();
        }

        bool exists(const string_type &path) const
        {
            return false;
        }

        basic_setting& add(const basic_setting& value)
        {
            value_ptr v(new value_type(value));
            v->m_parent = m_container;
            m_properties.push_back(v);
            return *m_properties.back();
        }

        void remove(size_t index)
        {
            if(index >= m_properties.size()) {
                throw _not_found_ex(index);
            }
            m_properties.erase(m_properties.begin() + index);
        }

        int indexOf(const basic_setting &child) const
        {
            typename std::vector<value_ptr>::const_iterator it=m_properties.begin();
            for(; it != m_properties.end(); ++it)
            {
                if (child == **it)
                    return true;
            }
            return false;
        }

        size_t size() const
        {
            return m_properties.size();
        }

        basic_setting* m_container;
        std::vector<value_ptr> m_properties;
        boost::regex rx_index;
    };

    class _basic_setting_container : public _basic_setting
    {
        _basic_setting_container(const _basic_setting_container&) {}
        _basic_setting_container& operator =(const _basic_setting_container&)
        { return *this; }

    public:
        typedef basic_setting<char_type> value_type;
        typedef boost::shared_ptr<value_type> value_ptr;

        _basic_setting_container(basic_setting* container)
            : m_container(container)
        {
        }

        bool operator==(const _basic_setting& other) const
        {
            const _basic_setting_container& o = static_cast<const _basic_setting_container&>(other);

            if(m_mapping.size() == o.m_mapping.size()) {
                typename std::map<string_type, value_ptr>::const_iterator lhs = m_mapping.begin();
                typename std::map<string_type, value_ptr>::const_iterator rhs = o.m_mapping.begin();
                for(; lhs != m_mapping.end(); ++lhs, ++rhs) {
                    if(!(*lhs->second == *rhs->second)) {
                        return false;
                    }
                }
                return true;
            }
            return false;
        }

        void print(std::ostream& o, size_t level) const
        {
            bool complex = m_container->m_parent || !m_container->m_name.empty();
            string_type ident_p(level * 4, ' ');
            size_t level_c = complex ? level + 1 : level;
            string_type ident_c(level_c * 4, ' ');
            if (complex)
                o << "{\n";

            typename std::map<string_type, value_ptr>::const_iterator it = m_mapping.begin();
            for(; it != m_mapping.end(); ++it)
            {
                o << ident_c;
                it->second->print(o, level_c);
                o << ";\n";
            }
            if (complex)
                o << ident_p << "}";
        }

        _basic_setting* clone(basic_setting *new_container)
        {
            _basic_setting_container* item = new _basic_setting_container(new_container);
            typename std::map<string_type, value_ptr>::iterator it = m_mapping.begin();
            for (; it != m_mapping.end(); ++it) {
                item->add(*it->second);
            }
            return item;
        }

        basic_setting& at(const string_type &property)
        {
            if(m_mapping.count(property)) {
                return *m_mapping.at(property);
            }
            throw _not_found_ex(property);
        }

        basic_setting& at(size_t index)
        {
            if(index < m_mapping.size()) {

                size_t counter = 0;
                typename std::map<string_type, value_ptr>::iterator it = m_mapping.begin();

                for(; it != m_mapping.end() && counter < index; ++it)
                    counter++;

                return *it->second;
            }

            throw _not_found_ex(index);
        }

        bool exists(size_t index) const
        {
            return index < m_mapping.size();
        }

        bool exists(const string_type& property) const
        {
            return m_mapping.count(property);
        }

        basic_setting& add(const basic_setting& value)
        {
            if (m_mapping.count(value.name())) {
                throw _name_ex(value.name() + " already exists");
            }
            value_ptr v(new value_type(value));
            v->m_parent = m_container;
            m_mapping.insert(std::make_pair(value.name(), v));
            return *v;
        }

        void remove(const string_type &property)
        {
            if (m_mapping.count(property)) {
                m_mapping.erase(property);
            }
            throw _not_found_ex(property);
        }

        void remove(size_t index)
        {
            const basic_setting& setting = at(index);
            m_mapping.erase(setting.name());
        }

        size_t size() const
        {
            return m_mapping.size();
        }

        basic_setting* m_container;
        std::map<string_type, value_ptr> m_mapping;
    };

    template<typename T>
    class _basic_setting_scalar : public _basic_setting
    {
    public:
        _basic_setting_scalar()
            : m_value(T())
        {}

        _basic_setting_scalar(const T& value)
            : m_value(value)
        {}

        _basic_setting* clone(basic_setting *new_container)
        {
            return new _basic_setting_scalar(*this);
        }

        bool operator==(const _basic_setting& other) const
        {
            const _basic_setting_scalar<T>& o = static_cast<const _basic_setting_scalar<T>& >(other);

            const T& lhs = boost::any_cast<T>(m_value);
            const T& rhs = boost::any_cast<T>(o.m_value);

            return lhs == rhs;
        }

        void print(std::ostream& o, size_t) const
        {
            o << boost::any_cast<T>(m_value);
        }

        void lookupValue(bool& result)
        {
            switch(_deduce_scalar_type(T())) {
            case TypeBoolean:
            case TypeInt:
            case TypeInt64:
            {
                long t;
                lookupValue(t);
                result = t != 0;
                break;
            }
            case TypeFloat:
            {
                float t = boost::any_cast<float>(m_value);
                result = t != 0;
                break;
            }
            default:
                throw _type_ex("unsupported conversion");
            }
        }

        void lookupValue(int& result)
        {
            switch(_deduce_scalar_type(T())) {
            case TypeBoolean:
            case TypeInt:
            case TypeInt64:
            {
                long t;
                lookupValue(t);

                if(t > std::numeric_limits<int>::max() || t < std::numeric_limits<int>::min()) {
                    throw _type_ex("type overflow");
                }
                result = t;
                break;
            }
            default:
                throw _type_ex("unsupported conversion");
            }
        }

        void lookupValue(unsigned int& result)
        {
            switch(_deduce_scalar_type(T())) {
            case TypeBoolean:
            case TypeInt:
            case TypeInt64:
            {
                long t;
                lookupValue(t);
                if(t < 0) {
                    throw _type_ex("negative value");
                } else if (t > std::numeric_limits<unsigned int>::max()) {
                    throw _type_ex("type overflow");
                }
                result = t;
                break;
            }
            default:
                throw _type_ex("unsupported conversion");
            }
        }

        void lookupValue(long& result)
        {
            switch(_deduce_scalar_type(T())) {
            case TypeBoolean:
            {
                bool t = boost::any_cast<bool>(m_value);
                if (t) {
                    result = 1;
                } else {
                    result = 0;
                }
                break;
            }
            case TypeInt:
                result = boost::any_cast<int>(m_value);
                break;
            case TypeInt64:
                result = boost::any_cast<long>(m_value);
                break;
            default:
                throw _type_ex("unsupported conversion");
            }
        }

        void lookupValue(unsigned long& result)
        {
            switch(_deduce_scalar_type(T())) {
            case TypeBoolean:
            case TypeInt:
            case TypeInt64:
            {
                long t;
                lookupValue(t);
                if (t >= 0) {
                    result = t;
                } else {
                    throw _type_ex("negative value");
                }
            }
            default:
                throw _type_ex("unsupported conversion");
            }
        }

        void lookupValue(float& result)
        {
            switch(_deduce_scalar_type(T())) {
            case TypeBoolean:
            case TypeInt:
            case TypeInt64:
            {
                long t;
                lookupValue(t);
                result = t;
                break;
            }
            case TypeFloat:
                result = boost::any_cast<float>(m_value);
                break;
            default:
                throw _type_ex("unsupported conversion");
            }
        }

        void lookupValue(double& result)
        {
            switch(_deduce_scalar_type(T())) {
            case TypeBoolean:
            case TypeInt:
            case TypeInt64:
            case TypeFloat:
                float t;
                lookupValue(t);
                result = t;
            default:
                throw _type_ex("unsupported conversion");
            }
        }

        void lookupValue(string_type& result)
        {
            switch(_deduce_scalar_type(T()))
            {
            case TypeString:
                result = boost::any_cast<string_type>(m_value);
                break;
            default:
                throw _type_ex("unsupported conversion");
            }
        }

        boost::any m_value;
    };

    static void _check_path(const string_type& path)
    {
        if(path.empty()) {
            throw std::invalid_argument("Path is empty");
        } else if (path[0] == '.' || path[path.size()-1] == '.') {
            throw std::invalid_argument("Path can not begin or end with dot(.)");
        }
    }

    static void _check_index(int index)
    {
        if(index < 0) {
            throw std::invalid_argument("Index can not be negative number");
        }
    }

    static SettingNameException _name_ex(const string_type& msg, const string_type& path = string_type())
    {
        return SettingNameException(msg, path);
    }
    static SettingNameException _name_ex(const SettingNameException& ex, const string_type& path = string_type())
    {
        return SettingNameException(ex.what(), path);
    }
    static SettingTypeException _type_ex(const string_type& msg,const string_type& path = string_type())
    {
        return SettingTypeException(msg, path);
    }
    static SettingTypeException _type_ex(const SettingTypeException& ex,const string_type& path = string_type())
    {
        return SettingTypeException(ex.what(), path);
    }
    static SettingNotFoundException _not_found_ex(const string_type& path = string_type())
    {
        return SettingNotFoundException("Setting not found", path);
    }
    static SettingNotFoundException _not_found_ex(size_t index)
    {
        std::ostringstream ss;
        ss << "[" << index << "]";
        return _not_found_ex(ss.str());
    }
    static SettingNotFoundException _not_found_ex(const SettingNotFoundException& ex, const string_type& path = string_type())
    {
        return SettingNotFoundException(ex.what(), path);
    }

    string_type m_name;
    Type m_type;
    basic_setting* m_parent;
    boost::scoped_ptr<_basic_setting> m_value;
};

template<typename charT>
class basic_config : public basic_setting<charT>
{
public:
    typedef charT char_type;
    typedef std::basic_string<charT> string_type;
    typedef std::vector<string_type> string_array;
    typedef basic_setting<char_type> value_type;
    typedef std::vector<value_type> value_array;
    typedef typename value_array::const_iterator value_iterator;
    typedef typename value_type::Type config_type;

    basic_config()
        : value_type(""),
          m_include_dir(boost::filesystem::current_path().generic_string())
    {}

    explicit basic_config(const char *path)
        : value_type(""),
          m_include_dir(boost::filesystem::current_path().generic_string())
    {
        value_type::operator =(_read_file(path));
    }

    explicit basic_config(const string_type& path)
        : value_type(""),
          m_include_dir(boost::filesystem::current_path().generic_string())
    {
        value_type::operator =(_read_file(path));
    }

    void readFile(const string_type& path)
    {
        value_type::operator =(_read_file(path));
    }

    void writeFile(const string_type& path)
    {
        string_type _path = _construct_path(path, m_include_dir);
        std::basic_ostream<char_type> ofs(_path);
        if (ofs) {
            ofs << *this;
        } else {
            throw FileIOException("Unable to open file " + _path);
        }
    }

    void setIncludeDir(const string_type& dir)
    {
        m_include_dir = dir;
    }

    string_type getIncludeDir() const
    {
        return m_include_dir;
    }

    basic_setting<char_type>& getRoot() const
    {
        return *this;
    }


private:
    class token;
    typedef std::vector<token> token_array;
    typedef typename token_array::const_iterator token_iterator;
    typedef boost::shared_ptr<string_type> string_ptr;
    typedef std::vector<string_ptr> files_list;

    string_type m_include_dir;

    class __basic_setting_list: public value_type
    {
    public:
        __basic_setting_list(const string_type &name, const value_array& values,
                             typename value_type::Type type)
            : value_type(name, values, type)
        {}
    };

    class token : public string_type
    {
    public:
        token() : string_type(), line(0), offset(0), file() {}

        token(const char_type* arg, size_t _line = 0, size_t _offset = 0)
            : string_type(arg), line(_line), offset(_offset), file()
        {}

        explicit token(char_type arg, size_t _line = 0, size_t _offset = 0)
            : string_type(), line(_line), offset(_offset), file()
        {
            *this += arg;
        }

        token(const string_type& arg, size_t _line = 0, size_t _offset = 0)
            : string_type(arg), line(_line), offset(_offset), file()
        {}

        template<typename T>
        token& operator=(const T& other)
        {
            string_type::operator =(other);
            return *this;
        }

        token& operator=(const token& other)
        {
            if (other != *this) {
                string_type::operator =(other);
                line = other.line;
                offset = other.offset;
                file = other.file;
            }
            return *this;
        }

        size_t line;
        size_t offset;
        string_ptr file;
    };

    class config_tokenizer
    {
    public:
        typedef char_type Char;

        config_tokenizer() : separators("{}[](),/\\\"=:;")
        {
            reset();
        }

        template<typename InputIterator, typename Token>
        bool operator()(InputIterator& next, InputIterator& end, Token &tok)
        {
            if(!tmp_token.empty()) {
                tok = tmp_token;
                tok.line = line;
                tok.offset = offset;
                tmp_token.resize(0);
                return true;
            }

            bool escape = false;
            bool is_identifier = false;
            while(next != end) {
                Char c = skip_comment(next, end);
                if (c == 0)
                    return false;
                if(is_string) {
                    if (c == '"' && !escape) {
                        tok += c;
                        is_string = false;
                        return true;
                    } else if(escape) {
                        escape = false;
                        switch(c) {
                        case '\\':
                        case '"':
                            tok += c;
                            break;
                        case 't':
                            tok += '\t';
                            break;
                        case 'n':
                            tok += '\n';
                            break;
                        default:
                            throw _syntax_exception(
                                        string_type("Unallowed escape token \\") + c,
                                        token(tok, line, offset));
                        }
                    } else if (c == '\\') {
                        escape = true;
                    } else {
                        tok += c;
                    }
                } else if(c == '"') {
                    is_string = true;
                    tok = c;
                    tok.line = line;
                    tok.offset = offset;
                } else if(is_identifier) {
                    if (isspace(c)) {
                        return true;
                    } else if(is_in_set(c, separators)) {
                        tmp_token = c;
                        return true;
                    } else {
                        tok += c;
                    }
                } else if (is_in_set(c, separators)) {
                    tok = c;
                    tok.line = line;
                    tok.offset = offset;
                    return true;
                } else if(!isspace(c)) {
                    tok = c;
                    tok.line = line;
                    tok.offset = offset;
                    is_identifier = true;
                }
            }
            return false;
        }

        bool is_in_set(Char c, const string_type& set)
        {
            for(size_t i = 0; i<set.size(); i++) {
                if (set[i] == c) {
                    return true;
                }
            }
            return false;
        }

        template<typename T>
        Char skip_comment(T& next, T&end)
        {
            bool simple_comment = false;
            bool complicated_comment = false;
            bool escape = false;
            while(next != end) {
                Char c = get(next);
                if(is_string) {
                    return c;
                } else if (complicated_comment) {
                    if (c == '/' && escape) {
                        complicated_comment = false;
                        escape = false;
                    } else if(c == '*') {
                        escape = true;
                    } else {
                        escape = false;
                    }
                } else if(simple_comment) {
                    if (c == '\n') {
                        simple_comment = false;
                    }
                } else if (c == '#') {
                    simple_comment = true;
                } else if (c == '/') {
                    if (next == end) {
                        throw _syntax_exception("Unexpected end of comment",
                                                token(c, line, offset));
                    }
                    c = get(next);
                    if (c == '*') {
                        complicated_comment = true;
                    } else if (c == '/') {
                        simple_comment = true;
                    } else {
                        throw _syntax_exception(string_type("Unexpected character ") + c,
                                                token(c, line, offset));
                    }
                } else {
                    return c;
                }
            }
            return 0;
        }

        template<typename T>
        Char get(T& s)
        {
            Char c = *s++;
            offset++;
            if(new_line) {
                line++;
                offset = 1;
                new_line = false;
            }
            if (c == '\n') {
                new_line = true;
            }
            return c;
        }

        void reset()
        {
            is_string = false;
            escape = false;
            new_line = false;
            line = 1;
            offset = 1;
            tmp_token.resize(0);
        }

    private:
        bool is_string;
        bool escape;
        bool new_line;
        size_t line;
        size_t offset;
        string_type tmp_token;
        string_type separators;
    };

    class parser
    {
        typedef config_tokenizer tok_func;
        typedef std::istream_iterator<char_type, char_type> char_iterator;
        typedef boost::tokenizer<tok_func, char_iterator, token> tokenizer;
        typedef typename tokenizer::iterator token_iterator;

    public:
        parser(const string_ptr& file, const string_type& include_dir,
               size_t level)
            : m_file(file),
              m_stream(file->c_str()),
              m_include_directory(include_dir),
              m_deep_level(level),
              m_tokenizer(get_iterator(m_stream), char_iterator(), tok_func()),
              it(m_tokenizer.begin()),
              end(m_tokenizer.end())
        {}

        /*!
         * \brief parse file(s)
         * \return tokens
         */
        token_array parse()
        {
            // replace include with tokens
            try {
                token_array tokens;
                bool is_include = false;
                while(it != end) {
                    token tok = *it++;
                    tok.file = m_file;
                    if(is_include) {
                        is_include = false;
                        token_array _tokens = include(tok);
                        tokens.insert(tokens.end(), _tokens.begin(), _tokens.end());
                    } else if (tok == "@include") {
                        is_include = true;
                    } else {
                        tokens.push_back(tok);
                    }
                }
                return tokens;
            } catch(ParseException& ex) {
                throw _syntax_exception(ex, m_file.get());
            }
        }

        /*!
         * \brief Parse file or files that match patern
         * \param _path path or pattern
         * \return tokens from parsed file(s)
         */
        token_array include(string_type _path)
        {
            using namespace boost;
            using namespace boost::filesystem;

            _path = _construct_path(_remove_quotes(_path), m_include_directory);

            files_list files;

            size_t delimiter = _path.find_last_of('/');
            if (delimiter == string_type::npos) {
                files.push_back(string_ptr(new string_type(_path)));
            } else if (delimiter+1 < _path.size()) {
                string_type directory = _path.substr(0, delimiter);
                regex pattern("^" + _path.substr(delimiter + 1) + "$");
                directory_iterator it(directory);
                directory_iterator end;

                while(it != end) {
                    path ptx = *it++;
                    string_type filename = ptx.filename().generic_string();
                    if (is_regular_file(ptx) && regex_match(filename, pattern)) {
                        files.push_back(string_ptr(new string_type(absolute(ptx).generic_string())));
                    }
                }
            } else {
                throw FileIOException("Can'f find file " + _path);
            }

            token_array tokens;
            for(size_t i = 0; i<files.size(); i++)
            {
                parser p(files[i], m_include_directory, m_deep_level + 1);
                token_array _tokens = p.parse();
                tokens.insert(tokens.end(), _tokens.begin(), _tokens.end());
            }
            return tokens;
        }

    private:

        char_iterator get_iterator(std::ifstream & stream)
        {
            stream >> std::noskipws;
            return char_iterator(stream);
        }

        string_ptr m_file;
        std::ifstream m_stream;
        string_type m_include_directory;
        size_t m_deep_level;
        tokenizer m_tokenizer;
        token_iterator it;
        token_iterator end;
    };

    static string_type _construct_path(const string_type& filename, const string_type& include_dir)
    {
        if (filename.empty()) {
            throw std::invalid_argument("Filename is empty");
        }

        if (filename[0] == '/') {
            return filename;
        } else {
            return include_dir + "/" + filename;
        }
    }

    template<typename T>
    /*!
     * \brief removes quotes at start and end, if such exist.
     * \param value value from which quotes should be removed
     * \return string without quotes at start and end
     */
    static T _remove_quotes(const T& value) {
        if (!value.empty() && value[0] == '"' && value.size() > 2) {
            return value.substr(1, value.size() - 2);
        }
        return value;
    }

    /*!
     * \brief concatenates adjacent strings
     * \param tokens
     * \return tokens with concatenated strings
     */
    token_array _concat_string(const token_array& tokens)
    {
        BOOST_ASSERT(!tokens.empty());

        token_array result;
        result.push_back(tokens.front());
        for(size_t i=1; i<tokens.size(); i++) {
            token& prev = result.back();
            const token& cur = tokens[i];
            if (prev[0] == '"' && cur[0] == '"') {
                prev = prev.substr(0, prev.size() - 1);
                prev += cur.substr(1);
            } else {
                result.push_back(cur);
            }
        }
        return result;
    }

    value_type _read_file(const string_type& path)
    {
        using namespace boost::filesystem;
        value_type root("");
        string_type _path = _construct_path(path, m_include_dir);

        parser p(string_ptr(new string_type(_path)), m_include_dir, 0);
        token_array tokens = p.parse();
        if (!tokens.empty()) {
            tokens = _concat_string(tokens);
            token_iterator begin = tokens.begin();
            token_iterator end = tokens.end();
            value_array settings = _get_setting_list(begin, end);
            for (size_t i = 0; i < settings.size(); i++) {
                root.add(settings[i]);
            }
        }
        return root;
    }

    value_array _get_setting_list(token_iterator& begin, token_iterator& end)
    {
        value_array settings;
        while(begin != end) {
            token tok = *begin++;
            if(tok == "[" || tok == "]") {
                throw _syntax_exception("unexpected array", tok);
            } else if(tok == "{" || tok == "}") {
                throw _syntax_exception("unexpected group setting without identifier", tok);
            } else if(tok == "," || tok == "=" || tok == ":") {
                throw _syntax_exception("unexpected token " + tok, tok);
            } else {
                settings.push_back(_get_setting(tok, begin, end));
            }
        }
        return settings;
    }

    value_type _get_setting(const token& identifier, token_iterator& begin, token_iterator& end)
    {
        if (begin != end) {
            token tok = *begin++;
            if (tok == "=" || tok == ":") {
                if (begin != end) {
                    tok = *begin;
                    if (tok == "{") {
                        value_type result = _get_group(identifier, begin, end);
                        begin = _skip_end(begin, end);
                        return result;
                    } else if (tok == "(") {
                        value_type result = _get_list(identifier, begin, end);
                        begin = _skip_end(begin, end);
                        return result;
                    } else if (tok == "[") {
                        value_type result = _get_array(identifier, begin, end);
                        begin = _skip_end(begin, end);
                        return result;
                    } else {
                        value_type result = _get_scalar_item(identifier, *begin++);
                        begin = _skip_end(begin, end);
                        return result;
                    }
                } else {
                    throw _syntax_exception("unexpected end of file", tok, true);
                }
            } else {
                throw _syntax_exception("unexpected token " + tok, tok);
            }
        } else {
            throw _syntax_exception("unexpected end of file", identifier, true);
        }
    }

    value_type _get_group(const token& identifier, token_iterator& begin, token_iterator& end)
    {
        token_iterator _begin = begin;
        token_iterator _end = _find_pair("{", "}", _begin++, end);

        begin = _end;
        if (begin != end) {
            ++begin;
        }

        value_type result(static_cast<string_type>(identifier));
        value_array settings = _get_setting_list(_begin, _end);

        for(size_t i=0; i<settings.size(); i++) {
            result.add(settings[i]);
        }
        return result;
    }

    value_type _get_list(const token& identifier, token_iterator& begin, token_iterator& end)
    {
        token_iterator _begin = begin;
        token_iterator _end = _find_pair("(", ")", _begin, end);

        begin = _end;
        if(begin != end) {
            ++begin;
        }

        std::vector<basic_setting<char_type> > values;

        token_iterator _last = _begin;
        while(_begin != _end) {
            _last = ++_begin;
            _begin = _find_list_item(_begin, _end);
            if(_last != _begin)
                values.push_back(_get_list_item(_last, _begin));
        };

        return __basic_setting_list(identifier, values, value_type::TypeList);
    }

    value_type _get_array(const token& identifier, token_iterator& begin, token_iterator& end)
    {
        token_iterator _begin = begin;
        token_iterator _end = _find_pair("[", "]", _begin, end);

        begin = _end;
        if(begin != end) {
            ++begin;
        }

        value_array values;

        token_iterator _last = begin;
        while(_begin != _end) {
            _last = ++_begin;
            _begin = _find_list_item(_begin, _end);
            if (_last != _begin)
                values.push_back(_get_list_item(_last, _begin));
        };

        return __basic_setting_list(identifier, values, value_type::TypeArray);
    }

    value_type _get_list_item(token_iterator begin, token_iterator end)
    {
        BOOST_ASSERT(begin != end);
        token tok = *begin;
        if(tok == "(") {
            return _get_list("", begin, end);
        } else if (tok == "{") {
            return _get_group("", begin, end);
        } else if (tok == "[") {
            return _get_array("", begin, end);
        } else {
            return _get_scalar_item("", tok);
        }
    }

    config_type _get_scalar_type(const token& value)
    {
        using namespace std;
        using namespace boost;

        regex rx_boolean("^([Tt][Rr][Uu][Ee])|([Ff][Aa][Ll][Ss][Ee])$");
        regex rx_int("^[-+]?[0-9]+$");
        regex rx_int64("^[-+]?[0-9]+L(L)?$");
        regex rx_hex("^0[Xx][0-9A-Fa-f]+$");
        regex rx_hex64("^0[Xx][0-9A-Fa-f]+L(L)?$");
        regex rx_float("^([-+]?([0-9]*)?\\.[0-9]*([eE][-+]?[0-9]+)?)|"
                       "([-+]?([0-9]+)(\\.[0-9]*)?[eE][-+]?[0-9]+)$");

        if (value[0] == '"') {
            return value_type::TypeString;
        } else if (regex_match(value, rx_boolean)) {
            return value_type::TypeBoolean;
        } else if (regex_match(value, rx_int)) {
            return value_type::TypeInt;
        } else if (regex_match(value, rx_hex)) {
            return value_type::TypeInt;
        } else if (regex_match(value, rx_int64)) {
            return value_type::TypeInt64;
        } else if (regex_match(value, rx_hex64)) {
            return value_type::TypeInt64;
        } else if (regex_match(value, rx_float)) {
            return value_type::TypeFloat;
        } else {
            return value_type::TypeGroup;
        }
    }

    value_type _get_scalar_item(const token& name, const token& value)
    {
        using namespace std;
        using namespace boost;

        regex rx_hex("^0[Xx][0-9A-Fa-f]+$");
        regex rx_hex64("^0[Xx][0-9A-Fa-f]+L(L)?$");

        switch(_get_scalar_type(value))
        {
        case value_type::TypeString:
        {
            string_type _value = value;
            return value_type(name, _value);
        }
        case value_type::TypeBoolean:
        {
            istringstream iss(value);
            bool v;
            iss >> v;
            return value_type(name, v);
        }
        case value_type::TypeInt:
        {
            istringstream iss(value);
            int v;
            if (regex_match(value, rx_hex))
                iss >> hex;
            iss >> v;
            return value_type(name, v);
        }
        case value_type::TypeInt64:
        {
            istringstream iss(value);
            long v;
            if (regex_match(value, rx_hex64))
                iss >> hex;
            iss >> v;
            return value_type(name, v);
        }
        case value_type::TypeFloat:
        {
            istringstream iss(value);
            float v;
            iss >> v;
            return value_type(name, v);
        }
        default:
            throw _syntax_exception("invalid value " + value, value);
        }
    }

    token_iterator _skip_end(token_iterator& begin, token_iterator& end)
    {
        if (begin != end && (*begin == ";" || *begin == ",")) {
            return ++begin;
        }
        return begin;
    }

    token_iterator _find_pair(const string_type& open_token, const string_type& close_token,
                              token_iterator begin, token_iterator end)
    {
        token_iterator _it(begin);
        size_t count = 0;
        while(_it != end) {
            token tok = *_it;
            if (tok == open_token) {
                count++;
            } else if(tok == close_token) {
                if (count == 1) {
                    return _it;
                } else {
                    count--;
                }
            }
            ++_it;
        }
        throw _syntax_exception("unable to find closing tag of " + *begin, *begin);
    }

    token_iterator _find_list_item(token_iterator begin, token_iterator end)
    {
        token_iterator _it(begin);
        size_t a_brace = 0;
        size_t g_brace = 0;
        size_t l_brace = 0;

        while(_it != end) {
            token tok = *_it;
            if (tok == "[") {
                a_brace++;
            } else if (tok == "]") {
                if (a_brace == 0)
                    throw _syntax_exception("unmatched array brace", tok);
                a_brace--;
            } else if (tok == "{") {
                g_brace++;
            } else if (tok == "}") {
                if (g_brace == 0)
                    throw _syntax_exception("unmatched group brace", tok);
                g_brace--;
            } else if (tok == "(") {
                l_brace++;
            } else if (tok == ")") {
                if (l_brace == 0)
                    throw _syntax_exception("unmatched list brace", tok);
                l_brace--;
            } else if (tok == "," && !a_brace && !g_brace && !l_brace) {
                return _it;
            }
            ++_it;
        }

        return end;
    }

    static ParseException _syntax_exception(const string_type& msg, const token& tok,
                                            bool offset_at_end_of_token = false)
    {
        std::ostringstream ss;
        size_t offset = tok.offset;
        if (offset_at_end_of_token)
            offset += tok.size();

        string_type file;
        if (tok.file) {
            file = *tok.file;
        }

        return ParseException(ss.str(), file, tok.line, tok.offset);
    }

    static ParseException _syntax_exception(const ParseException& ex, const string_type* file)
    {
        return ParseException(ex.what(), *file, ex.line(), ex.offset());
    }
};

template<typename CharT>
std::ostream& operator<<(std::ostream &o, const basic_setting<CharT>& rhs)
{
    rhs.print(o, 0);
    return o;
}

typedef basic_setting<char> Setting;
typedef basic_config<char> Config;

}

#endif // LIBCONFIGPP_H
