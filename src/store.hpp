#pragma once
#include "sqlite3.h"
#include <string>
#include <stdexcept>
#include <vector>
using Row=std::vector<std::string>;
using Rows=std::vector<Row>;
class Statement {
    sqlite3_stmt* stmt=nullptr;
public:
    Statement(sqlite3* db,const std::string& sql){
        if(sqlite3_prepare_v2(db,sql.c_str(),-1,&stmt,nullptr)!=SQLITE_OK)
            throw std::runtime_error(sqlite3_errmsg(db));
    }
    ~Statement(){sqlite3_finalize(stmt);}
    Statement(const Statement&)=delete;
    Rows run(const Row& args){
        for(size_t i=0;i<args.size();++i)
            if(sqlite3_bind_text(stmt,int(i+1),args[i].c_str(),-1,SQLITE_TRANSIENT)!=SQLITE_OK)
                throw std::runtime_error("Bind failed");
        Rows result;
        int code;
        while((code=sqlite3_step(stmt))==SQLITE_ROW){
            Row row;
            for(int c=0;c<sqlite3_column_count(stmt);++c){
                auto value=sqlite3_column_text(stmt,c);
                row.emplace_back(value?reinterpret_cast<const char*>(value):"");
            }
            result.push_back(row);
        }
        if(code!=SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(sqlite3_db_handle(stmt)));
        return result;
    }
};
class Store {
    sqlite3* db=nullptr;
public:
    Store(){
        if(sqlite3_open("data/hotel.db",&db)!=SQLITE_OK) throw std::runtime_error("Cannot open hotel database");
        sqlite3_busy_timeout(db,5000);
    }
    Store(const Store&)=delete;
    ~Store(){sqlite3_close(db);}
    Rows query(const std::string& sql,const Row& args={}){return Statement(db,sql).run(args);}
};
