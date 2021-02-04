#pragma once
#include "Misc.h"
#include <iostream>
#include <cassert>
#include <map>
#include <vector>

class SaveObjectMap;
class SaveObjectList;
class SaveObjectNull;

class SaveObject
{
public:
    SaveObject(){};
    virtual ~SaveObject(){};
    virtual void save(std::ostream& f)=0;
    static SaveObject* load(std::istream& f);
    virtual int get_num(){assert(0);return 0;};
    virtual std::string get_string(){assert(0);return "";};
    virtual SaveObjectMap* get_map(){assert(0);return NULL;};
    virtual SaveObjectList* get_list(){assert(0);return NULL;};
    virtual bool is_null(){return false;};
};

class SaveObjectNumber :
    public SaveObject
{
public:
    double number;
    SaveObjectNumber(int number_):number(number_){};
    SaveObjectNumber(std::istream& stream) {stream >> number;};
    int get_num(){return number;};
    double get_float(){return number;};
    void save(std::ostream& f){f << number;};
};

class SaveObjectString :
    public SaveObject
{
public:
    std::string str;
    SaveObjectString(std::string str_):str(str_){};
    SaveObjectString(std::istream& stream);
    std::string get_string();
    void save(std::ostream& f);
};

class SaveObjectMap :
    public SaveObject
{
public:
    std::map<std::string, SaveObject*> omap;

    SaveObjectMap(){};
    virtual ~SaveObjectMap();
    SaveObjectMap(std::istream& f);
    void save(std::ostream& f);
    SaveObjectMap* get_map(){return this;};
    
    void add_item(std::string key, SaveObject* value);
    SaveObject* get_item(std::string key);
    int get_num(std::string key);
    void get_num(std::string key, int& value);
    void add_num(std::string key, int value);
    void add_string(std::string key, std::string value);
    void get_string(std::string key, std::string& value);

};


class SaveObjectList :
    public SaveObject
{
public:
    std::vector<SaveObject*> olist;

    SaveObjectList(){};
    SaveObjectList(std::istream& f);
    ~SaveObjectList();
    void save(std::ostream& f);
    SaveObjectList* get_list(){return this;};
    
    void add_item(SaveObject* value);
    SaveObject* get_item(unsigned index);
    unsigned get_count();
    void add_num(int value);
    int get_num(int index);
    void add_string(std::string value);
    std::string get_string(int index);

};


class SaveObjectNull :
    public SaveObject
{
public:
    SaveObjectNull(){};
    SaveObjectNull(std::istream& f);
    void save(std::ostream& f);
    virtual bool is_null(){return true;};
};
