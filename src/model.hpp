#pragma once
#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>
#include <cstdio>
using std::string;
struct InputError : std::runtime_error { using runtime_error::runtime_error; };
using Day = std::chrono::sys_days;
inline Day parseDay(const string& s) {
    int y,m,d;
    if (s.size()!=10 || sscanf(s.c_str(),"%4d-%2d-%2d",&y,&m,&d)!=3)
        throw InputError("Please choose valid dates.");
    std::chrono::year_month_day date{std::chrono::year(y),std::chrono::month(unsigned(m)),std::chrono::day(unsigned(d))};
    char canonical[32];
    snprintf(canonical,sizeof canonical,"%04d-%02d-%02d",y,m,d);
    if(!date.ok() || s!=canonical) throw InputError("Please choose valid dates.");
    return Day(date);
}
inline auto hotelTime(){return std::chrono::system_clock::now()+std::chrono::minutes(330);} 
inline string dateString(Day day){
    std::chrono::year_month_day date{day};
    char buf[32]; snprintf(buf,sizeof buf,"%04d-%02u-%02u",int(date.year()),unsigned(date.month()),unsigned(date.day()));
    return buf;
}
inline Day today(){return std::chrono::floor<std::chrono::days>(hotelTime());}
class Room {
    int number_;
    string bed_;
    string type_;
    int rate_;
public:
    Room(int number, string bed, string type, int rate):number_(number),bed_(std::move(bed)),type_(std::move(type)),rate_(rate){}
    virtual ~Room() = default;
    int number() const {return number_;}
    int floor() const {return number_ / 100;}
    string bed() const {return bed_;}
    virtual int nightlyRate() const {return rate_;}
    virtual string type() const {return type_;}
    int quote(int nights) const {return nights*nightlyRate();}
};
class NonACSingleRoom final : public Room {
public:
    explicit NonACSingleRoom(int number):Room(number,"Single bed","Non-AC",1099){}
};
class NonACDoubleRoom final : public Room {
public:
    explicit NonACDoubleRoom(int number):Room(number,"Double bed","Non-AC",1999){}
};
class ACSingleRoom final : public Room {
public:
    explicit ACSingleRoom(int number):Room(number,"Single bed","AC",1499){}
};
class ACDoubleRoom final : public Room {
public:
    explicit ACDoubleRoom(int number):Room(number,"Double bed","AC",2499){}
};
struct Slot {string name; int start,end; string hours;};
struct Dish {string name; bool veg; int price,slot; string unit; string description;};
class Hotel {
public:
    std::vector<std::unique_ptr<Room>> rooms;
    const std::vector<Slot> slots{
        {"Breakfast",7,11,"7–11 AM"},
        {"Lunch",12,15,"12–3 PM"},
        {"Snacks & Beverages",16,19,"4–7 PM"},
        {"Dinner",19,22,"7–10 PM"}
    };
    // Menu rates are demonstration prices and can be edited here.
    const std::vector<Dish> dishes{
        {"Idli (2 pieces)",true,60,0,"plate","Soft steamed idlis with chutney and sambar"},
        {"Poori (2 pieces)",true,85,0,"plate","Fresh poori with potato curry"},
        {"Masala dosa",true,110,0,"plate","Crispy dosa with potato masala"},
        {"Plain dosa",true,85,0,"plate","Dosa served with chutney and sambar"},
        {"Upma",true,75,0,"plate","Warm savoury breakfast"},
        {"Pepper omelette",false,130,0,"portion","Egg omelette with black pepper"},
        {"Veg meals",true,180,1,"plate","Rice, dal, curry, sambar and sides"},
        {"Bagara rice",true,160,1,"plate","Spiced Andhra-style bagara rice"},
        {"Bagara rice & chicken curry",false,310,1,"combo","Bagara rice served with chicken curry"},
        {"Chicken curry",false,220,1,"bowl","Chicken curry served separately"},
        {"Chicken biryani",false,280,1,"plate","Spiced rice with chicken"},
        {"Paneer rice bowl",true,210,1,"bowl","Rice and paneer gravy"},
        {"Samosa (2 pieces)",true,60,2,"plate","Crispy potato-filled samosas"},
        {"Onion pakoda",true,90,2,"plate","Crispy onion fritters"},
        {"Veg sandwich",true,100,2,"portion","Toasted vegetable sandwich"},
        {"French fries",true,120,2,"portion","Golden crispy fries"},
        {"Masala tea",true,35,2,"cup","Indian spiced milk tea"},
        {"Coffee",true,50,2,"cup","Hot freshly brewed coffee"},
        {"Phulka with paneer",true,190,3,"plate","Soft phulkas with paneer curry"},
        {"Phulka with chicken",false,240,3,"plate","Soft phulkas with chicken curry"},
        {"Chicken curry",false,220,3,"bowl","Chicken curry ordered separately"},
        {"Paneer curry",true,170,3,"bowl","Paneer gravy ordered separately"},
        {"Chapathi",true,25,3,"piece","Select how many individual chapathis you want"},
        {"Plain phulka",true,20,3,"piece","Fresh soft phulka; select pieces"},
        {"Dal tadka & roti",true,190,3,"plate","Dal and roti meal"},
        {"Chicken curry & rice",false,260,3,"plate","Chicken curry served with rice"}
    };
    Hotel(){
        for(int n=101;n<=107;++n) rooms.push_back(std::make_unique<NonACSingleRoom>(n));
        for(int n=201;n<=203;++n) rooms.push_back(std::make_unique<NonACSingleRoom>(n));
        for(int n=204;n<=206;++n) rooms.push_back(std::make_unique<ACDoubleRoom>(n));
        for(int n=301;n<=307;++n) rooms.push_back(std::make_unique<ACSingleRoom>(n));
        for(int n=401;n<=406;++n) rooms.push_back(std::make_unique<ACDoubleRoom>(n));
    }
    const Room& room(const string& n) const {
        for(auto& r:rooms) if(std::to_string(r->number())==n) return *r;
        throw InputError("Unknown room.");
    }
};
