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
    virtual int64_t get_num(){throw(std::runtime_error("Not a num"));};
    virtual std::string get_string(){throw(std::runtime_error("Not a string"));};
    virtual SaveObjectMap* get_map(){throw(std::runtime_error("Not a map"));};
    virtual SaveObjectList* get_list(){throw(std::runtime_error("Not a list"));};
    virtual bool is_null(){return false;};
    virtual bool is_map(){return false;};
    virtual bool is_num(){return false;};
    virtual bool is_string(){return false;};
    virtual bool is_list(){return false;};
    
    virtual SaveObject* dup() = 0;
};

class SaveObjectNumber :
    public SaveObject
{
public:
    int64_t number;
    SaveObjectNumber(int64_t number_):number(number_){};
    SaveObjectNumber(std::istream& stream) {stream >> number;};
    int64_t get_num(){return number;};
    void save(std::ostream& f){f << number;};
    SaveObject* dup() {return new SaveObjectNumber(number);};
    virtual bool is_num(){return true;};
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
    SaveObject* dup() {return new SaveObjectString(str);};
    virtual bool is_string(){return true;};
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
    int64_t get_num(std::string key);
    void get_num(std::string key, int& value);
    void add_num(std::string key, int64_t value);
    void add_string(std::string key, std::string value);
    void get_string(std::string key, std::string& value);
    std::string get_string(std::string key);
    bool has_key(std::string key);
    SaveObject* dup();
    virtual bool is_map(){return true;};

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
    void add_num(int64_t value);
    int64_t get_num(int index);
    void add_string(std::string value);
    std::string get_string(int index);
    SaveObject* dup();
    virtual bool is_list(){return true;};

};


class SaveObjectNull :
    public SaveObject
{
public:
    SaveObjectNull(){};
    SaveObjectNull(std::istream& f);
    void save(std::ostream& f);
    virtual bool is_null(){return true;};
    SaveObject* dup() {return new SaveObjectNull();};
};
