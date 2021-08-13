#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <list>
#include <stdexcept>
#include <signal.h>
#include <codecvt>
#include <locale>
#include <time.h>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "Compress.h"
#include "SaveState.h"
#include "Level.h"

class Database;

class Score
{
public:
    int64_t score = 0;
    std::string c_save;
    unsigned version = 0;
    Score(){}
    Score(const Score& other)
    {
        score = other.score;
        c_save = other.c_save;
        version = other.version;
    }
    ~Score()
    {
    }
    void update_design(SaveObject* sobj, unsigned version_)
    {
        c_save = compress_string(sobj->to_string());
        version = version_;
    }

    void update_design(std::string st, unsigned version_)
    {
        c_save = st;
        version = version_;
    }

    SaveObject* get_design(unsigned& version_)
    {
        std::string s = decompress_string(c_save);
        version_ = version;
        return SaveObject::load(s);
    }
};
    

class ScoreTable
{
public:
    std::multimap<int64_t, uint64_t, std::greater<int64_t>> sorted_scores;
    std::map<uint64_t, Score> user_score;
    
    ~ScoreTable()
    {
   
    }
    
    SaveObject* save(bool lite)
    {
        SaveObjectList* score_list = new SaveObjectList;
        for(auto const &score : sorted_scores)
        {
            SaveObjectMap* score_map = new SaveObjectMap;
            uint64_t id = score.second;
            score_map->add_num("id", id);
            score_map->add_num("score", user_score[id].score);
            if (!lite)
                score_map->add_string("design", user_score[id].c_save);
            score_map->add_num("version", user_score[id].version);
            score_list->add_item(score_map);
        }
        return score_list;
    }
    
    void load(SaveObject* sobj)
    {
        SaveObjectList* score_list = sobj->get_list();
        for (unsigned i = 0; i < score_list->get_count(); i++)
        {
            SaveObjectMap* omap = score_list->get_item(i)->get_map();
            std::string des = omap->get_string("design");
            unsigned load_game_version = 0;
            if (omap->has_key("version"))
                load_game_version = omap->get_num("version");
            add_score(omap->get_num("id"), omap->get_num("score"), des, load_game_version);
        }
    }

    int64_t get_score(uint64_t steam_id)
    {
        return user_score[steam_id].score;
    }

    void add_score(uint64_t steam_id, int64_t score, SaveObject* sobj, unsigned version)
    {
        if ((user_score.find(steam_id) != user_score.end()) && (score < user_score[steam_id].score))
            return;

        if (score == user_score[steam_id].score)
        {
            user_score[steam_id].update_design(sobj, version);
            return;
        }

        for (auto it = sorted_scores.begin(); it != sorted_scores.end(); )
        {
           if (it->second == steam_id)
           {
                if (it->first >= score)
                    return;
                it = sorted_scores.erase(it);
                break;
           }
           else
               ++it;
        }
        sorted_scores.insert({score, steam_id});
        user_score[steam_id].score = score;
        user_score[steam_id].update_design(sobj, version);
    }

    void add_score(uint64_t steam_id, int64_t score, std::string& sobj, unsigned version)
    {
        if ((user_score.find(steam_id) != user_score.end()) && (score < user_score[steam_id].score))
            return;

        if (score == user_score[steam_id].score)
        {
            user_score[steam_id].update_design(sobj, version);
            return;
        }

        for (auto it = sorted_scores.begin(); it != sorted_scores.end(); )
        {
           if (it->second == steam_id)
           {
                if (it->first >= score)
                    return;
                it = sorted_scores.erase(it);
                break;
           }
           else
               ++it;
        }
        sorted_scores.insert({score, steam_id});
        user_score[steam_id].score = score;
        user_score[steam_id].update_design(sobj, version);
    }

   void fetch_scores(SaveObjectMap* omap, uint64_t user_id, std::set<uint64_t>& friends, Database& db, unsigned type, int visible);

    SaveObject* get_design(uint64_t steam_id, unsigned& version_)
    {
        if (!user_score.count(steam_id))
            throw(std::runtime_error("bad user id"));
        return user_score[steam_id].get_design(version_);
    }
    
    void clear()
    {
        sorted_scores.clear();
        user_score.clear();
    }
    
};

class Player
{
public:
    std::string steam_username;
    int top_level = 0;
};

class CustomLevel
{
public:
    std::string name;
    int level_index;
    unsigned version = 0;
    SaveObject* sobj = NULL;
    ScoreTable accuracy_scores;
    ScoreTable price_scores;
    ScoreTable steam_scores;
    time_t release_time;
    
    CustomLevel()
    {
    }
    CustomLevel(std::string name_, int level_index_, SaveObject* sobj_, unsigned version_)
    {
        name = name_;
        level_index = level_index_;
        sobj = sobj_->dup();
        version = version_;
        release_time = time(NULL) + 60*60*24*7;
    }

    CustomLevel(const CustomLevel& other)
    {
        name = other.name;
        level_index = other.level_index;
        if (other.sobj)
            sobj = other.sobj->dup();
        accuracy_scores = other.accuracy_scores;
        price_scores = other.price_scores;
        steam_scores = other.steam_scores;
        release_time = other.release_time;
        version = other.version;
    }

    ~CustomLevel()
    {
        delete sobj;
    }

    SaveObject* save(bool lite)
    {
        SaveObjectMap* omap = new SaveObjectMap;
        
        omap->add_item("accuracy_scores", accuracy_scores.save(lite));
        omap->add_item("price_scores", price_scores.save(lite));
        omap->add_item("steam_scores", steam_scores.save(lite));
        
        omap->add_num("version", version);
        omap->add_item("design", sobj->dup());
        omap->add_string("name", name);
        omap->add_num("level_index", level_index);
        omap->add_num("release_time", release_time);

        return omap;
    }

    void load(SaveObject* sobjin)
    {
        SaveObjectMap* omap = sobjin->get_map();
        name = omap->get_string("name");
        level_index = omap->get_num("level_index");
        release_time = omap->get_num("release_time");
        if (omap->has_key("version"))
            version = omap->get_num("version");
        sobj = omap->get_item("design")->dup();
        accuracy_scores.load(omap->get_item("accuracy_scores"));
        price_scores.load(omap->get_item("price_scores"));
        steam_scores.load(omap->get_item("steam_scores"));
   }
        

};


class Database
{
public:
    std::map<uint64_t, Player> players;
    std::vector<ScoreTable> levels;
    std::vector<ScoreTable> levels_price;
    std::vector<ScoreTable> levels_steam;
    std::list<CustomLevel> custom_levels;

    std::map<uint64_t, std::string> paste_designs;

    void update_name(uint64_t steam_id, std::string& steam_username)
    {
        players[steam_id].steam_username = steam_username;
    }


//     void update_priv(uint64_t steam_id, bool priv)
//     {
//         players[steam_id].priv = priv;
//     }
// 
    void update_score(uint64_t steam_id, unsigned level, int64_t score, SaveObject* sobj, unsigned version)
    {
        if (level > players[steam_id].top_level)
        {
            players[steam_id].top_level = level;
            printf("New level reached: %s %d\n", players[steam_id].steam_username.c_str(), level);
        }
        if (level >= levels.size())
            levels.resize(level + 1);
        levels[level].add_score(steam_id, score, sobj, version);
    }

    void update_price(uint64_t steam_id, unsigned level, int64_t score, SaveObject* sobj, unsigned version)
    {
        if (level >= levels_price.size())
            levels_price.resize(level + 1);
        levels_price[level].add_score(steam_id, INT64_MAX - score, sobj, version);
    }

    void update_steam(uint64_t steam_id, unsigned level, int64_t score, SaveObject* sobj, unsigned version)
    {
        if (level >= levels_steam.size())
            levels_steam.resize(level + 1);
        levels_steam[level].add_score(steam_id, INT64_MAX - score, sobj, version);
    }

    void load(SaveObject* sobj)
    {
        SaveObjectMap* omap = sobj->get_map();
        SaveObjectList* player_list = omap->get_item("players")->get_list();
        for (unsigned i = 0; i < player_list->get_count(); i++)
        {
            SaveObjectMap* omap = player_list->get_item(i)->get_map();
            std::string name;
            uint64_t id = omap->get_num("id");
            omap->get_string("steam_username", name);
            int top_level = omap->get_num("top_level");
            update_name(id, name);
            players[id].top_level = top_level;
//             if (omap->has_key("chat"))
//             {
//                 SaveObjectList* chat_list = omap->get_item("chat")->get_list();
//                 for (unsigned i = 0; i < chat_list->get_count(); i++)
//                 {
//                     
//                 }
//             }
        }

        SaveObjectList* level_list = omap->get_item("levels")->get_list();
        if (level_list->get_count() > levels.size())
            levels.resize(level_list->get_count());
        for (unsigned i = 0; i < level_list->get_count(); i++)
        {
            SaveObject* sobj = level_list->get_item(i);
            levels[i].load(sobj);
        }

        if (omap->has_key("levels_price"))
        {
            SaveObjectList* level_list = omap->get_item("levels_price")->get_list();
            if (level_list->get_count() > levels_price.size())
                levels_price.resize(level_list->get_count());
            for (unsigned i = 0; i < level_list->get_count(); i++)
            {
                SaveObject* sobj = level_list->get_item(i);
                levels_price[i].load(sobj);
            }
        }

        if (omap->has_key("levels_steam"))
        {
            SaveObjectList* level_list = omap->get_item("levels_steam")->get_list();
            if (level_list->get_count() > levels_steam.size())
                levels_steam.resize(level_list->get_count());
            for (unsigned i = 0; i < level_list->get_count(); i++)
            {
                SaveObject* sobj = level_list->get_item(i);
                levels_steam[i].load(sobj);
            }
        }

        if (omap->has_key("levels_steam"))
        {
            SaveObjectList* level_list = omap->get_item("levels_steam")->get_list();
            if (level_list->get_count() > levels_steam.size())
                levels_steam.resize(level_list->get_count());
            for (unsigned i = 0; i < level_list->get_count(); i++)
            {
                SaveObject* sobj = level_list->get_item(i);
                levels_steam[i].load(sobj);
            }
        }

        if (omap->has_key("custom_levels"))
        {
            SaveObjectList* level_list = omap->get_item("custom_levels")->get_list();
            for (unsigned i = 0; i < level_list->get_count(); i++)
            {
                custom_levels.push_back(CustomLevel());
                custom_levels.back().load(level_list->get_item(i));
            }
        }

        if (omap->has_key("pastes"))
        {
            SaveObjectList* level_list = omap->get_item("pastes")->get_list();
            for (unsigned i = 0; i < level_list->get_count(); i++)
            {
                SaveObjectMap* paste_map = level_list->get_item(i)->get_map();
                uint64_t paste_id = paste_map->get_num("paste_id");
                paste_designs[paste_id] = paste_map->get_string("design");
            }
        }

    }

    SaveObject* save(bool lite)
    {
        SaveObjectMap* omap = new SaveObjectMap;
        
        SaveObjectList* player_list = new SaveObjectList;
        
        for(auto &player_pair : players)
        {
            SaveObjectMap* player_map = new SaveObjectMap;
            player_map->add_num("id", player_pair.first);
//            player_map->add_num("priv", player_pair.second.priv);
            player_map->add_string("steam_username", player_pair.second.steam_username);
            player_map->add_num("top_level", player_pair.second.top_level);
//             SaveObjectList* chat_list = new SaveObjectList;
//             for (ChatMessage& msg : player_pair.second.messages)
//             {
//                 SaveObjectMap* message_map = new SaveObjectMap;
//                 message_map->add_num("steam_id", msg.steam_id);
//                 message_map->add_string("text", msg.text);
//                 chat_list->add_item(message_map);
//             }
//             player_map->add_item("chat", chat_list);

            player_list->add_item(player_map);
        }
        omap->add_item("players", player_list);

        unsigned highest = levels.size();
        SaveObjectList* level_list = new SaveObjectList;
        for (int i = 0; i < highest; i++)
        {
            level_list->add_item(levels[i].save(lite));
        }
        omap->add_item("levels", level_list);

        highest = levels_price.size();
        level_list = new SaveObjectList;
        for (int i = 0; i < highest; i++)
        {
            level_list->add_item(levels_price[i].save(lite));
        }
        omap->add_item("levels_price", level_list);

        highest = levels_steam.size();
        level_list = new SaveObjectList;
        for (int i = 0; i < highest; i++)
        {
            level_list->add_item(levels_steam[i].save(lite));
        }
        omap->add_item("levels_steam", level_list);

        level_list = new SaveObjectList;
        for (CustomLevel &clevel :custom_levels)
        {
            level_list->add_item(clevel.save(lite));
        }
        omap->add_item("custom_levels", level_list);

        level_list = new SaveObjectList;
        for(auto &paste_pair : paste_designs)
        {
            SaveObjectMap* paste_map = new SaveObjectMap;
            paste_map->add_num("paste_id", paste_pair.first);
            paste_map->add_string("design", paste_pair.second);
            level_list->add_item(paste_map);
        }
        omap->add_item("pastes", level_list);

        return omap;
    }

    SaveObject* get_scores(unsigned level, uint64_t user_id, std::set<uint64_t>& friends, unsigned type = 0)
    {
        std::vector<ScoreTable> *level_ptr = &levels;
        if (type == 1)
            level_ptr = &levels_price;
        else if (type == 2)
            level_ptr = &levels_steam;
        
        if (level >= LEVEL_COUNT)
            throw(std::runtime_error("bad level id"));
        if (level >= level_ptr->size())
            level_ptr->resize(level + 1);
        SaveObjectMap* omap = new SaveObjectMap;
        omap->add_num("level", level);
        omap->add_num("score", type ? (INT64_MAX - (*level_ptr)[level].get_score(user_id)) : (*level_ptr)[level].get_score(user_id));
        omap->add_num("type", type);
        (*level_ptr)[level].fetch_scores(omap, user_id, friends, *this, type, 0);
        return omap;
    }

    SaveObject* get_scores(std::string name, uint64_t user_id, std::set<uint64_t>& friends, unsigned type = 0)
    {
        for (CustomLevel &clevel :custom_levels)
        {
            if (clevel.name == name)
            {
                ScoreTable* table = &clevel.accuracy_scores;
                if (type == 1)
                    table = &clevel.price_scores;
                else if (type == 2)
                    table = &clevel.steam_scores;

                SaveObjectMap* omap = new SaveObjectMap;
                omap->add_string("name", name);
                omap->add_num("score", type ? (INT64_MAX - table->get_score(user_id)) : table->get_score(user_id));
                omap->add_num("type", type);
                if (clevel.release_time > time(NULL))
                    friends.clear();
                table->fetch_scores(omap, user_id, friends, *this, type, 100);
                return omap;
            }
        }
        return new SaveObjectMap;
    }

    SaveObject* get_design(uint64_t level_steam_id, unsigned level_index, unsigned type = 0)
    {
        std::vector<ScoreTable> *level_ptr = &levels;
        if (type == 1)
            level_ptr = &levels_price;
        else if (type == 2)
            level_ptr = &levels_steam;

        if (level_index >= LEVEL_COUNT)
            throw(std::runtime_error("bad level id"));
        if (level_index >= level_ptr->size())
            level_ptr->resize(level_index + 1);
        SaveObjectMap* omap = new SaveObjectMap;
        omap->add_num("level_index", level_index);
        unsigned version;
        omap->add_item("levels", (*level_ptr)[level_index].get_design(level_steam_id, version));
        omap->add_num("version", version);
        omap->add_num("steam_id", level_steam_id);
        omap->add_string("steam_username", players[level_steam_id].steam_username);
        return omap;
    }

    SaveObject* get_design(uint64_t level_steam_id, std::string name, unsigned type = 0)
    {
        for (CustomLevel &clevel :custom_levels)
        {
            if (clevel.name == name)
            {
                ScoreTable* table = &clevel.accuracy_scores;
                if (type == 1)
                    table = &clevel.price_scores;
                else if (type == 2)
                    table = &clevel.steam_scores;

                SaveObjectMap* omap = new SaveObjectMap;

                omap->add_num("level_index", clevel.level_index);
                omap->add_string("level_name", clevel.name);
                unsigned version;
                omap->add_item("levels", table->get_design(level_steam_id, version));
                omap->add_num("version", version);
                omap->add_num("steam_id", level_steam_id);
                omap->add_string("steam_username", players[level_steam_id].steam_username);
                return omap;
            }
        }
        return new SaveObjectMap;
    }

    SaveObject* get_server_level_design(std::string name)
    {
        for (CustomLevel &clevel :custom_levels)
        {
            if (clevel.name == name)
            {
                SaveObjectMap* omap = new SaveObjectMap;

                omap->add_num("level_index", clevel.level_index);
                omap->add_num("version", clevel.version);
                omap->add_item("levels", clevel.sobj->dup());
                return omap;
            }
        }
        return new SaveObjectMap;
    }

    SaveObject* get_custom_levels(uint64_t user_id)
    {
        SaveObjectList* resp = new SaveObjectList;

        for (CustomLevel &clevel :custom_levels)
        {
            SaveObjectMap* omap = new SaveObjectMap;
            omap->add_string("name", clevel.name);
            resp->add_item(omap);
        }
        return resp;
    }

    void remove_custom_level(std::string name)
    {
        for (auto it = custom_levels.begin(); it != custom_levels.end(); ++it)
        {
            if ((*it).name == name)
            {
                custom_levels.erase(it);
                break;
            }
        }
    }

    void add_custom_level(int level_index, SaveObject* sobj, std::string name)
    {
        custom_levels.push_back(CustomLevel(name, level_index, sobj, COMPRESSURE_VERSION));
    }

    void update_custom_score(std::string name, uint64_t steam_id, unsigned level, int64_t score, SaveObject* sobj, unsigned version)
    {
        for (CustomLevel &clevel :custom_levels)
        {
            if (clevel.name == name)
            {
                clevel.accuracy_scores.add_score(steam_id, score, sobj, version);
            }
        }
    }

    void update_custom_price(std::string name, uint64_t steam_id, unsigned level, int64_t score, SaveObject* sobj, unsigned version)
    {
        for (CustomLevel &clevel :custom_levels)
        {
            if (clevel.name == name)
            {
                clevel.price_scores.add_score(steam_id, INT64_MAX - score, sobj, version);
            }
        }
    }

    void update_custom_steam(std::string name, uint64_t steam_id, unsigned level, int64_t score, SaveObject* sobj, unsigned version)
    {
        for (CustomLevel &clevel :custom_levels)
        {
            if (clevel.name == name)
            {
                clevel.steam_scores.add_score(steam_id, INT64_MAX - score, sobj, version);
            }
        }
    }

    bool reinit_tests(Level* level)
    {
        if (!level->global)
            return false;
        for (CustomLevel &clevel :custom_levels)
        {
            if (clevel.name == level->name)
            {
                level->re_init_tests(clevel.sobj->get_list()->get_item(clevel.level_index)->get_map());
                return true;
            }
        }
        return false;
    }
};



class Workload
{
public:
    virtual ~Workload(){};
    virtual bool execute() = 0;
};

class SubmitScore : public Workload
{
public:
    LevelSet* level_set;
    int current_level = 0;
    bool init_level = false;
    std::string steam_username;
    uint64_t steam_id;
    Database& db;
    
    SubmitScore(Database& db_, SaveObjectMap* omap):
        db(db_)
    {
        int level_index = omap->get_num("level_index");
        unsigned version = 0;
        if (omap->has_key("version"))
            version = omap->get_num("version");
        level_set = new LevelSet(omap->get_item("levels"), version, true);
        omap->get_string("steam_username", steam_username);
        steam_id = omap->get_num("steam_id");
        db.update_name(steam_id, steam_username);
    }

    ~SubmitScore()
    {
        delete level_set;
    }
    void update_scores()
    {
        for (unsigned level_index = 0; level_index < 10000; level_index++)
        {
            if (level_set->is_playable(level_index, LEVEL_COUNT))
            {
                Pressure score = level_set->levels[level_index]->last_score;
                if (level_index >= LEVEL_COUNT)
                {
                    if (level_set->levels[level_index]->global)
                    {
                        SaveObject* save_object = level_set->save_one(level_index);
                        if (score)
                        {
                            db.update_custom_score(level_set->levels[level_index]->name, steam_id, level_index, level_set->levels[level_index]->last_score, save_object, COMPRESSURE_VERSION);
                            db.update_custom_price(level_set->levels[level_index]->name, steam_id, level_index, level_set->levels[level_index]->last_price, save_object, COMPRESSURE_VERSION);
                            db.update_custom_steam(level_set->levels[level_index]->name, steam_id, level_index, level_set->levels[level_index]->last_steam, save_object, COMPRESSURE_VERSION);
                            printf("New score:%s - %f\n", level_set->levels[level_index]->name.c_str(), (float)score/65536);
                        }
                        delete save_object;
                    }
                }
                else if (score)
                {
                    SaveObject* save_object = level_set->save_one(level_index);
                    db.update_score(steam_id, level_index, level_set->levels[level_index]->last_score, save_object, COMPRESSURE_VERSION);
                    db.update_price(steam_id, level_index, level_set->levels[level_index]->last_price, save_object, COMPRESSURE_VERSION);
                    db.update_steam(steam_id, level_index, level_set->levels[level_index]->last_steam, save_object, COMPRESSURE_VERSION);

                    printf("New score:%d - %f\n", level_index, (float)score/65536);
                    delete save_object;
                }
            }
        }
    }

    bool execute()
    {
        while (!level_set->is_playable(current_level, LEVEL_COUNT))
        {
            current_level++;
            if (current_level >= 10000)
            {
                update_scores();
                return true;
            }
        }
        if (!init_level)
        {
            if (current_level >= LEVEL_COUNT)
            {
                if (!db.reinit_tests(level_set->levels[current_level]))
                {
                    current_level++;
                    return false;
                }
            }
            level_set->levels[current_level]->circuit->elaborate(level_set);

            level_set->reset(current_level);
            level_set->levels[current_level]->last_score = 0;
            level_set->levels[current_level]->best_score = 0;
            init_level = true;
        }
        level_set->levels[current_level]->advance(1000);
        if (level_set->levels[current_level]->score_set)
        {
            current_level++;
            init_level = false;
        }
        return false;
    }
};


class Connection
{
public:
    int conn_fd;
    int length;
    std::string inbuf;
    std::string outbuf;
    Connection(int conn_fd_):
        conn_fd(conn_fd_),
        length(-1)
    {
    }

    Workload* recieve(Database& db)
    {
        Workload* resp = NULL;
        static char buf[1024];
        while (true)
        {
            ssize_t num_bytes_received = recv(conn_fd, buf, sizeof(buf), MSG_DONTWAIT);
            if (num_bytes_received  == -1)
            {
                if (errno != EAGAIN && errno != EWOULDBLOCK)
                    close();
                break;
            }
            else if (num_bytes_received  == 0)
            {
                close();
                break;
            }
            inbuf.append(buf, num_bytes_received);
        }

        while (true)
        {
            ssize_t num_bytes_received = send(conn_fd, outbuf.c_str(), outbuf.length(), MSG_DONTWAIT);
            if (num_bytes_received  == -1)
            {
                if (errno != EAGAIN && errno != EWOULDBLOCK)
                    close();
                break;
            }
            else if (num_bytes_received  == 0)
            {
                break;
            }
            outbuf.erase(0, num_bytes_received);
        }
        while (true)
        {
            if (length < 0 && inbuf.length() >= 4)
            {
                length = *(uint32_t*)inbuf.c_str(),
                inbuf.erase(0, 4);
                if (length > 1024*1024)
                {
                    close();
                    break;
                }
            }
            else if (length > 0 && inbuf.length() >= length)
            {
                try
                {
                    std::string decomp = decompress_string(inbuf);
                    std::istringstream decomp_stream(decomp);
                    SaveObjectMap* omap = SaveObject::load(decomp_stream)->get_map();
                    inbuf.erase(0, length);
                    length = -1;

                    std::string command;
                    omap->get_string("command", command);
                    if (command == "save")
                    {
                        std::string steam_username;
                        omap->get_string("steam_username", steam_username);
                        db.update_name(omap->get_num("steam_id"), steam_username);
                        printf("save: %s %lld\n", steam_username.c_str(), omap->get_num("steam_id"));
                        std::ofstream outfile (steam_username.c_str());
                        omap->save(outfile);

                        std::ostringstream stream;
                        omap->get_item("content")->save(stream);
                        std::string comp = compress_string(stream.str());
                        std::u32string s32;
                        std::string reply;

                        s32 += '\n';
                        s32 += 0x1F682;                 // steam engine
                        unsigned spaces = 2;
                        for(char& c : comp)
                        {
                            if (spaces >= 80)
                            {
                                s32 += '\n';
                                spaces = 0;
                            }
                            spaces++;
                            s32 += uint32_t(0x2800 + (unsigned char)(c));

                        } 
                        s32 += 0x1F6D1;                 // stop sign

                        std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> conv;
                        reply += conv.to_bytes(s32);
                        reply += "\n";
                        outfile << reply;
                        outfile.close();
                    }
                    else if (command == "score_submit")
                    {
                        std::string steam_username;
                        omap->get_string("steam_username", steam_username);
                        db.update_name(omap->get_num("steam_id"), steam_username);
                        printf("score_submit: %s %lld\n", steam_username.c_str(), omap->get_num("steam_id"));
                        resp = new SubmitScore(db, omap);
                    }
                    else if (command == "global_design_submit" && omap->get_num("steam_id") == CHARLES_ID)
                    {
                        std::string steam_username;
                        int level_index = omap->get_num("level_index");
                        unsigned load_game_version = 0;
                        if (omap->has_key("version"))
                            load_game_version = omap->get_num("version");
                        LevelSet* level_set = new LevelSet(omap->get_item("levels"), load_game_version, true);
                        omap->get_string("steam_username", steam_username);
                        db.update_name(omap->get_num("steam_id"), steam_username);
                        SaveObject* save_object = level_set->save_one(level_index);
                        db.remove_custom_level(level_set->levels[level_index]->name);
                        if (level_set->levels[level_index]->global)
                            db.add_custom_level(level_index, save_object, level_set->levels[level_index]->name);
                        delete save_object;
                        delete level_set;

                        printf("global_design_submit: %s %lld\n", steam_username.c_str(), omap->get_num("steam_id"));
                    }
                    else if (command == "paste_submit")
                    {
                        std::string steam_username = omap->get_string("steam_username");
                        uint64_t paste_id = omap->get_num("paste_id");
                        SaveObject* save_object = omap->get_item("levels");
                        db.paste_designs[paste_id] = compress_string(omap->to_string());
                        printf("paste_submit: %s %llu\n", steam_username.c_str(), paste_id);
                    }
                    else if (command == "score_fetch")
                    {
                        std::string steam_username;
                        omap->get_string("steam_username", steam_username);
                        db.update_name(omap->get_num("steam_id"), steam_username);
                        printf("score_fetch: %s %lld\n", steam_username.c_str(), omap->get_num("steam_id"));
                        std::set<uint64_t> friends;
                        unsigned type = omap->get_num("type");
                        if (omap->has_key("friends"))
                        {
                            SaveObjectList* friend_list = omap->get_item("friends")->get_list();
                            for (unsigned i = 0; i < friend_list->get_count(); i++)
                            {
                                friends.insert(friend_list->get_num(i));
                            }
                            friends.insert(omap->get_num("steam_id"));
                        }
                        SaveObject* scores;
                        if (omap->has_key("level_index"))
                            scores = db.get_scores(omap->get_num("level_index"), omap->get_num("steam_id"), friends, type);
                        else
                            scores = db.get_scores(omap->get_string("name"), omap->get_num("steam_id"), friends, type);
                        std::ostringstream stream;
                        scores->save(stream);
                        std::string comp = compress_string_zlib(stream.str());
                        uint32_t length = comp.length();
                        outbuf.append((char*)&length, 4);
                        outbuf.append(comp);
                        delete scores;
                    }
                    else if (command == "design_fetch")
                    {
                        std::string steam_username;
                        omap->get_string("steam_username", steam_username);
                        db.update_name(omap->get_num("steam_id"), steam_username);
                        uint64_t level_steam_id = omap->get_num("level_steam_id");
                        unsigned type = omap->get_num("type");
                        printf("design_fetch: %s %lld  req %lld\n", steam_username.c_str(), omap->get_num("steam_id"), level_steam_id);
                        SaveObject* design;
                        if (omap->has_key("level_index"))
                            design = db.get_design(level_steam_id, omap->get_num("level_index"), type);
                        else
                            design = db.get_design(level_steam_id, omap->get_string("name"), type);
                        std::ostringstream stream;
                        design->save(stream);
                        std::string comp = compress_string_zlib(stream.str());
                        uint32_t length = comp.length();
                        outbuf.append((char*)&length, 4);
                        outbuf.append(comp);
                        delete design;
                    }
                    else if (command == "paste_fetch")
                    {
                        std::string steam_username;
                        omap->get_string("steam_username", steam_username);
                        uint64_t paste_id = omap->get_num("paste_id");
                        printf("paste_fetch: %s %lld \n", steam_username.c_str(), paste_id);
                        if (!db.paste_designs[paste_id].empty())
                        {
                            std::string dec = decompress_string(db.paste_designs[paste_id]);
                            std::string comp = compress_string_zlib(dec);
                            uint32_t length = comp.length();
                            outbuf.append((char*)&length, 4);
                            outbuf.append(comp);
                        }
                    }
                    else if (command == "server_levels_fetch")
                    {
                        std::string steam_username;
                        omap->get_string("steam_username", steam_username);
                        db.update_name(omap->get_num("steam_id"), steam_username);
                        printf("server_levels_fetch: %s %lld\n", steam_username.c_str(), omap->get_num("steam_id"));

                        SaveObject* custom_levels = db.get_custom_levels(omap->get_num("steam_id"));
                        std::ostringstream stream;
                        custom_levels->save(stream);
                        std::string comp = compress_string_zlib(stream.str());
                        uint32_t length = comp.length();
                        outbuf.append((char*)&length, 4);
                        outbuf.append(comp);
                        delete custom_levels;
                    }
                    else if (command == "server_level_fetch")
                    {
                        std::string steam_username;
                        omap->get_string("steam_username", steam_username);
                        db.update_name(omap->get_num("steam_id"), steam_username);
                        std::string name = omap->get_string("name");

                        printf("server_level_design_fetch: %s %lld  req %s\n", steam_username.c_str(), omap->get_num("steam_id"), name.c_str());
                        SaveObject* design;
                        design = db.get_server_level_design(name);
                        std::ostringstream stream;
                        design->save(stream);
                        std::string comp = compress_string_zlib(stream.str());
                        uint32_t length = comp.length();
                        outbuf.append((char*)&length, 4);
                        outbuf.append(comp);
                        delete design;
                    }
                    else if (command == "help_fetch")
                    {
                        omap->save(std::cout);
                        close();
                    }
                    else
                    {
                        printf("unknown command: %s \n", command.c_str());
                        close();
                    }
                    
                    delete omap;
                }
                catch (const std::runtime_error& error)
                {
                    std::cerr << error.what() << "\n";
                    close();
                    break;
                }
            }
            else
                break;
        }
        return resp;
    }
    
    void close()
    {
        if (conn_fd < 0)
            return;
        shutdown(conn_fd, SHUT_WR);
        ::close(conn_fd);
        conn_fd = -1;
    }

};

bool power_down = false;

void sig_handler(int signo)
{
    if (signo == SIGUSR1)
        printf("received SIGUSR1\n");
    else if (signo == SIGTERM)
        printf("received SIGTERM\n");
    power_down = true;
    printf("shutting down\n");
}

int main(int argc, char *argv[])
{
    Database db;
    signal(SIGUSR1, sig_handler);
    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);

    try 
    {
        std::ifstream loadfile("db.save");
        if (!loadfile.fail() && !loadfile.eof())
        {
            SaveObjectMap* omap = SaveObject::load(loadfile)->get_map();
            db.load(omap);
            delete omap;
        }
    }
    catch (const std::runtime_error& error)
    {
        std::cerr << error.what() << "\n";
    }

    if (argc >= 2) 
    {
        db.levels[atoi(argv[1])].clear();
        db.levels_price[atoi(argv[1])].clear();
        db.levels_steam[atoi(argv[1])].clear();
    }




    struct sockaddr_in myaddr ,clientaddr;
    int sockid;
    sockid=socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK , 0);
//    memset(&myaddr,'0',sizeof(myaddr));
    myaddr.sin_family=AF_INET;
    myaddr.sin_port=htons(42069);
    myaddr.sin_addr.s_addr=INADDR_ANY;
    if(sockid==-1)
    {
        perror("socket");
        return 1;
    }
    socklen_t len=sizeof(myaddr);
    if(bind(sockid,( struct sockaddr*)&myaddr,len)==-1)
    {
        perror("bind");
        return 1;
    }
    if(listen(sockid,10)==-1)
    {
        perror("listen");
        return 1;
    }
    
    
    std::list<Connection> conns;
    std::list<Workload*> workloads;

    time_t old_time = 0;

    while(true)
    {
        if (workloads.empty())
        {
            fd_set w_fds;
            fd_set r_fds;
            struct timeval timeout;
            timeout.tv_sec = 5;
            timeout.tv_usec = 0;

            FD_ZERO(&r_fds);
            FD_ZERO(&w_fds);
            FD_SET(sockid, &r_fds);
            
            for (Connection &conn :conns)
            {
                FD_SET(conn.conn_fd, &r_fds);
                if (!conn.outbuf.empty())
                    FD_SET(conn.conn_fd, &w_fds);
            }
            
            select(1024, &r_fds, &w_fds, NULL, &timeout);
        }

        while (true)
        {
            int conn_fd = accept(sockid,(struct sockaddr *)&clientaddr, &len);
            if (conn_fd == -1)
                break;
            if (conn_fd >= 1024)
                ::close(conn_fd);
            conns.push_back(conn_fd);
        }

        for (std::list<Connection>::iterator it = conns.begin(); it != conns.end();)
        {
            Connection& conn = (*it);
            Workload* new_workload = conn.recieve(db);
            if (new_workload)
            {
                workloads.push_back(new_workload);
            }
            if (conn.conn_fd < 0)
            {
                it = conns.erase(it);
            }
            else
                it++;
        }

        for (std::list<Workload*>::iterator it = workloads.begin();it != workloads.end();)
        {
            Workload* workload = (*it);
            if (workload->execute())
            {
                delete workload;
                it = workloads.erase(it);
            }
            else
                it++;
        }

        fflush(stdout);
        if (power_down)
            break;

        time_t new_time;
        time(&new_time);
        if ((old_time + 60) < new_time)
        {
            old_time = new_time;
            std::ofstream outfile ("db.save");
            SaveObject* savobj = db.save(false);
            savobj->save(outfile);
            delete savobj;
            std::ofstream outfile_lite ("db.save.lite");
            SaveObject* savobj_lite = db.save(true);
            savobj_lite->save(outfile_lite);
            delete savobj_lite;
        }
    }
    close(sockid);
    {
        std::ofstream outfile ("db.save");
        SaveObject* savobj = db.save(false);
        savobj->save(outfile);
        delete savobj;
        std::ofstream outfile_lite ("db.save.lite");
        SaveObject* savobj_lite = db.save(true);
        savobj_lite->save(outfile_lite);
        delete savobj_lite;
    }
    delete level_desc;
    return 0;
}

void ScoreTable::fetch_scores(SaveObjectMap* omap, uint64_t user_id, std::set<uint64_t>& friends, Database& db, unsigned type, int visible)
{
    SaveObjectList* friend_scores = new SaveObjectList;

    std::vector<int64_t> scores;

    int i = 0;
    for(auto const &score : sorted_scores)
    {
        if (user_id && !score.second)
            continue;
        int64_t s = type ? (INT64_MAX - score.first) : score.first;

        scores.push_back(s);
        bool fri = (friends.find(score.second) != friends.end()) || (score.second == user_id);
        if ((user_id == CHARLES_ID )|| fri || (i < visible))
        {
            if (score.second == CHARLES_ID || score.second == 0)
                continue;
            SaveObjectMap* omap = new SaveObjectMap;
            omap->add_num("steam_id", score.second);
            omap->add_string("steam_username", db.players[score.second].steam_username);
            omap->add_num("score", s);
            omap->add_num("visible", fri || (user_id == CHARLES_ID));
            friend_scores->add_item(omap);
        }
        i++;
    }

    SaveObjectList* score_list = new SaveObjectList;

    unsigned count = scores.size();
    for (unsigned i = 0; i < 200; i++)
    {
        int64_t result = 0;
        if (count)
            result = scores[(i * count) / 200];
        score_list->add_num(result);

    }
    omap->add_item("graph", score_list);
    omap->add_item("friend_scores", friend_scores);
}
