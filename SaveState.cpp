#include "Misc.h"
#include "SaveState.h"
#include <assert.h>

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
    assert(0);
}

SaveObjectString::SaveObjectString(std::istream& f)
{
    char c;
    assert(f.get() == '\"');
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
    assert(f.get() == '{');
    char c;
    while (true)
    {
        if (f.peek() == '}')
            break;
        assert(f.get() == '\"');
        std::string key;
        while ((c = f.get()) != '\"')
            key.push_back(c);
        assert(f.get() == ':');
        SaveObject* obj = SaveObject::load(f);
        add_item(key, obj);
        if (f.peek() == '}')
            break;
        assert(f.get() == ',');
    }
    assert(f.get() == '}');
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
    return omap[key];
}

int SaveObjectMap::get_num(std::string key)
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
void SaveObjectMap::add_num(std::string key, int value)
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


SaveObjectList::SaveObjectList(std::istream& f)
{
    assert(f.get() == '[');
    char c;
    while (true)
    {
        if (f.peek() == ']')
            break;
        SaveObject* obj = SaveObject::load(f);
        add_item(obj);
        if (f.peek() == ']')
            break;
        assert(f.get() == ',');
    }
    assert(f.get() == ']');
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
    assert(index < get_count());
    return olist[index];
}

unsigned SaveObjectList::get_count()
{
    return olist.size();
}

void SaveObjectList::add_num(int value)
{
    add_item(new SaveObjectNumber(value));
}

int SaveObjectList::get_num(int index)
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

SaveObjectNull::SaveObjectNull(std::istream& f)
{
    assert(f.get() == 'n');
    assert(f.get() == 'u');
    assert(f.get() == 'l');
    assert(f.get() == 'l');
}

void SaveObjectNull::save(std::ostream& f)
{
    f << "null";
}
