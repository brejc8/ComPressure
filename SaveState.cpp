#include "Misc.h"
#include "SaveState.h"
#include <assert.h>
#define assert_exp(x) do { if (f.get() != x) throw (std::runtime_error("Unexpected character"));} while (false)

SaveObject* SaveObject::load(std::istream& f)
{
    char c = f.peek();
    if (c == '{')
        return new SaveObjectMap(f);
    if (c == 'n')
        return new SaveObjectNull(f);
    if (c == '[')
        return new SaveObjectList(f);
    if (c == '"')
        return new SaveObjectString(f);
    if ((c >= '0' && c <= '9') || c == '-')
        return new SaveObjectNumber(f);
    throw(std::runtime_error("Parse Error"));
}

SaveObjectString::SaveObjectString(std::istream& f)
{
    char c;
    assert_exp('\"');
    while ((c = f.get()) != '\"')
    {
        if (c == '\\')
        {
            c = f.get();
            if (c == 'n')
                c = '\n';
        }
        
        str.push_back(c);
    }
}
std::string SaveObjectString::get_string()
{
    return str;
}

void SaveObjectString::save(std::ostream& f)
{
    f << '"';
    for(std::string::iterator it = str.begin(); it != str.end(); ++it)
    {
        char c = *it;
        if (c == '\n')
            f << "\\n";
        else if (c == '"')
            f << "\\\"";
        else
            f << c;
    }
    f << '"';
}

SaveObjectMap::SaveObjectMap(std::istream& f)
{
    assert_exp('{');
    char c;
    while (true)
    {
        if (f.peek() == '}')
            break;
        assert_exp('\"');
        std::string key;
        while ((c = f.get()) != '\"')
            key.push_back(c);
        assert_exp(':');
        SaveObject* obj = SaveObject::load(f);
        add_item(key, obj);
        if (f.peek() == '}')
            break;
        assert_exp(',');
    }
    assert_exp('}');
}
SaveObjectMap::~SaveObjectMap()
{
    for(std::map<std::string, SaveObject*>::iterator it = omap.begin();it != omap.end();++it)
        delete it->second;
}

void SaveObjectMap::add_item(std::string key, SaveObject* value)
{
    omap[key]=value;
}

SaveObject* SaveObjectMap::get_item(std::string key)
{
    if (omap.find(key) == omap.end())
        throw(std::runtime_error("Bad map key"));
    return omap[key];
}

int64_t SaveObjectMap::get_num(std::string key)
{
    if (omap.find(key) != omap.end())
        return omap[key]->get_num();
    std::cout << "failed indexing for an int with key:" << key << "\n";
    return 0;
}
void SaveObjectMap::get_num(std::string key, int& value)
{
    if (omap.find(key) != omap.end())
        value = omap[key]->get_num();
}
void SaveObjectMap::add_num(std::string key, int64_t value)
{
    add_item(key, new SaveObjectNumber(value));
}
void SaveObjectMap::add_string(std::string key, std::string value)
{
    add_item(key, new SaveObjectString(value));
}
void SaveObjectMap::get_string(std::string key, std::string& value)
{
    if (omap.find(key) != omap.end())
        value = omap[key]->get_string();
}

std::string SaveObjectMap::get_string(std::string key)
{
    if (omap.find(key) != omap.end())
        throw(std::runtime_error("Bad map key"));
    return omap[key]->get_string();
}

bool SaveObjectMap::has_key(std::string key)
{
    return omap.find(key) != omap.end();
}

void SaveObjectMap::save(std::ostream& f)
{
    f.put('{');
    bool first = true;
    for (std::map<std::string, SaveObject*>::iterator it=omap.begin(); it!=omap.end(); ++it)
    {
        if (!first)
            f << ',';
        first = false;
        f << '"';
        f << it->first;
        f << '"';

        f << ':';
        it->second->save(f);
    }
    f.put('}');
};

SaveObject* SaveObjectMap::dup()
{
    SaveObjectMap* rep = new SaveObjectMap;
    for (std::map<std::string, SaveObject*>::iterator it=omap.begin(); it!=omap.end(); ++it)
    {
        rep->add_item(it->first, it->second->dup());
    }
    return rep;
};

SaveObjectList::SaveObjectList(std::istream& f)
{
    assert_exp('[');
    char c;
    while (true)
    {
        if (f.peek() == ']')
            break;
        SaveObject* obj = SaveObject::load(f);
        add_item(obj);
        if (f.peek() == ']')
            break;
        assert_exp(',');
    }
    assert_exp(']');
}

SaveObjectList::~SaveObjectList()
{
    for(std::vector<SaveObject*>::iterator it = olist.begin(); it != olist.end(); it++)
        delete *it;
}

void SaveObjectList::add_item(SaveObject* value)
{
    olist.push_back(value);
}

SaveObject* SaveObjectList::get_item(unsigned index)
{
    if (index >= get_count())
        throw(std::runtime_error("Bad list index"));
    return olist[index];
}

unsigned SaveObjectList::get_count()
{
    return olist.size();
}

void SaveObjectList::add_num(int64_t value)
{
    add_item(new SaveObjectNumber(value));
}

int64_t SaveObjectList::get_num(int index)
{
    return get_item(index)->get_num();
}

void SaveObjectList::save(std::ostream& f)
{
    f.put('[');
    bool first = true;
    for (std::vector<SaveObject*>::iterator it=olist.begin(); it!=olist.end(); ++it)
    {
        if (!first)
            f << ',';
        first = false;
        (*it)->save(f);
    }
    f.put(']');
}

SaveObject* SaveObjectList::dup()
{
    SaveObjectList* rep = new SaveObjectList;
    for (std::vector<SaveObject*>::iterator it=olist.begin(); it!=olist.end(); ++it)
    {
        rep->add_item((*it)->dup());
    }
    return rep;
};


SaveObjectNull::SaveObjectNull(std::istream& f)
{
    assert_exp('n');
    assert_exp('u');
    assert_exp('l');
    assert_exp('l');
}

void SaveObjectNull::save(std::ostream& f)
{
    f << "null";
}
