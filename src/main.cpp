#include "httplib.h"
#include "model.hpp"
#include "store.hpp"
#include <iostream>
#include <filesystem>
#include <cstdlib>
#include <regex>
#include <algorithm>
#include <cctype>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
using namespace std::string_literals;
using std::to_string;
using Req=httplib::Request;
using Res=httplib::Response;
Hotel hotel;
string param(const Req& q,const string& key){return q.get_param_value(key);}
// All credentials are validated on the C++ server; no Gmail/SMS service is connected.
string escapeHtml(const string& value){
    string out;
    for(char c:value){
        switch(c){
            case '&': out+="&amp;";break;
            case '<': out+="&lt;";break;
            case '>': out+="&gt;";break;
            case '"': out+="&quot;";break;
            case '\'': out+="&#39;";break;
            default:out+=c;
        }
    }
    return out;
}
string trim(string s){
    size_t a=s.find_first_not_of(" \t\r\n");
    if(a==string::npos)return "";
    size_t b=s.find_last_not_of(" \t\r\n");
    return s.substr(a,b-a+1);
}
string lower(string s){
    std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    return s;
}
string normalizedPhone(string s){
    s=trim(s);
    if(s.rfind("+91",0)==0) s=s.substr(3);
    else if(s.size()==12 && s.rfind("91",0)==0) s=s.substr(2);
    return s;
}
string hexBytes(const unsigned char* bytes,size_t count){
    static const char* hex="0123456789abcdef";
    string result;
    result.reserve(count*2);
    for(size_t i=0;i<count;++i){result.push_back(hex[bytes[i]>>4]);result.push_back(hex[bytes[i]&15]);}
    return result;
}
string secureRandomHex(size_t count){
    std::vector<unsigned char> bytes(count);
    if(RAND_bytes(bytes.data(),int(bytes.size()))!=1) throw std::runtime_error("Random source unavailable");
    return hexBytes(bytes.data(),bytes.size());
}
std::vector<unsigned char> parseHex(const string& hex){
    auto nibble=[](char c)->int {
        if(c>='0' && c<='9')return c-'0';
        if(c>='a' && c<='f')return c-'a'+10;
        return -1;
    };
    if(hex.size()%2)throw std::runtime_error("Malformed salt");
    std::vector<unsigned char> out;
    for(size_t i=0;i<hex.size();i+=2){
        int hi=nibble(hex[i]),lo=nibble(hex[i+1]);
        if(hi<0||lo<0) throw std::runtime_error("Malformed salt");
        out.push_back(static_cast<unsigned char>((hi<<4)|lo));
    }
    return out;
}
string passwordHash(const string& password,const string& saltHex){
    auto salt=parseHex(saltHex);
    unsigned char digest[32]={};
    if(PKCS5_PBKDF2_HMAC(password.c_str(),int(password.size()),salt.data(),int(salt.size()),
            210000,EVP_sha256(),sizeof(digest),digest)!=1)
        throw std::runtime_error("Password security error");
    auto hash=hexBytes(digest,sizeof(digest));
    OPENSSL_cleanse(digest,sizeof(digest));
    return hash;
}
bool passwordMatches(const string& password,const string& salt,const string& expected){
    if(expected.size()!=64)return false;
    string actual=passwordHash(password,salt);
    bool valid=CRYPTO_memcmp(actual.data(),expected.data(),64)==0;
    return valid;
}
string rawSessionToken(const Req& q){
    static const std::regex cookie(R"((?:^|;\s*)kingpin_session=([0-9a-f]{64})(?:;|$))");
    std::smatch m;
    string cookies=q.get_header_value("Cookie");
    return std::regex_search(cookies,m,cookie)?m[1].str():"";
}
// Returns a stable owner id: bookings remain visible after logging in on another browser.
string session(const Req& q){
    string token=rawSessionToken(q);
    if(token.empty())return "";
    Store db;
    auto match=db.query("SELECT account_id FROM auth_sessions WHERE token=? AND expires>unixepoch()",{token});
    return match.empty()?"":"acct-"+match[0][0];
}
string newLoginSession(Store& db,const string& accountId){
    string token=secureRandomHex(32);
    db.query("INSERT INTO auth_sessions(token,account_id,expires) VALUES(?,?,unixepoch()+2592000)",
            {token,accountId});
    return token;
}
void setLoginCookie(Res& r,const string& token){
    r.set_header("Set-Cookie","kingpin_session="+token+"; HttpOnly; SameSite=Strict; Path=/; Max-Age=2592000");
}
string authPage(bool registering,const string& message=""){
    string headline=registering?"Create your account.":"Welcome back.";
    string sub=registering?"Enter your name, Gmail/email, mobile number and a password to register.":
        "Sign in with your Gmail/email or mobile number and password.";
    string action=registering?"/register":"/login";
    string fields;
    if(registering){
        fields="<label>Full name<input name='name' type='text' autocomplete='name' required minlength='2' maxlength='80' placeholder='Your full name'></label>"
               "<label>Gmail / email address<input name='email' type='email' autocomplete='email' required maxlength='254' placeholder='name@gmail.com'></label>"
               "<label>Mobile number<input name='phone' type='tel' autocomplete='tel' required pattern='[6-9][0-9]{9}' maxlength='10' placeholder='10-digit mobile number'></label>";
    }else{
        fields="<label>Gmail / email or mobile number<input name='identity' type='text' autocomplete='username' required maxlength='254' placeholder='name@gmail.com or 9876543210'></label>";
    }
    fields+="<label>Password<input name='password' type='password' autocomplete='"+string(registering?"new-password":"current-password")+"' required minlength='10' maxlength='128' placeholder='Your password'></label>";
    if(registering){
        fields+="<label>Confirm password<input name='confirm' type='password' autocomplete='new-password' required minlength='10' maxlength='128' placeholder='Repeat your password'></label>";
    }
    return "<div class='auth-layout'><div class='auth-showcase'><p class='eyebrow'>HOTEL KINGPIN</p><h1>Make every stay memorable.</h1>"
           "<p>Four floors. Beautiful rooms. Dining delivered to your door.</p><div class='auth-benefits'>"
           "<span>✓ Secure account</span><span>✓ Your reservations</span><span>✓ Room service</span></div></div>"
           "<section class='auth-card'><p class='eyebrow'>"+string(registering?"JOIN THE KINGPIN CLUB":"GUEST SIGN-IN")+"</p><h2>"+headline+"</h2><p>"+sub+"</p>"
           +(message.empty()?"":"<p class='auth-error' role='alert'>"+escapeHtml(message)+"</p>")
           +"<form method='post' action='"+action+"' class='auth-form'>"+fields+
           "<button type='submit'>"+string(registering?"Create account":"Sign in")+"</button></form>"
           +"<p class='auth-switch'>"+string(registering?"Already have an account? <a href='/login'>Sign in</a>":
               "New to HOTEL KINGPIN? <a href='/register'>Create an account</a>")+"</p>"
           "<p class='auth-note'>This local project uses password login. Gmail and phone ownership are not verified; Google Sign-In and SMS/email OTP are not enabled.</p>"
           "</section></div>";
}
string hidden(string key,string value){return "<input type='hidden' name='"+key+"' value='"+value+"'>";}
string shell(string title,string body){
    // Every page shares a fixed header and a slide-out navigation drawer.
    return "<!doctype html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>"+escapeHtml(title)+" · HOTEL KINGPIN</title>"
        "<link rel='stylesheet' href='/static/style.css'>"
        "<script src='/static/side-nav.js' defer></script></head><body>"
        "<header class='site-header'><div class='header-inner'>"
        "<button type='button' id='menu-toggle' class='menu-toggle' aria-label='Open navigation menu' aria-controls='kingpin-drawer' aria-expanded='false'>"
        "<span aria-hidden='true'>☰</span><span>Menu</span></button>"
        "<a class='brand' href='/'>HOTEL KINGPIN<span>HOTEL &amp; KITCHEN</span></a>"
        "<nav class='desktop-nav' aria-label='Quick links'><a href='/#rooms'>Rooms</a><a href='/menu'>Dining</a><a href='/stays'>My stays</a><a href='/account'>Account</a></nav>"
        "</div></header>"
        "<button class='drawer-overlay' type='button' id='drawer-overlay' aria-label='Close navigation menu' tabindex='-1' hidden></button>"
        "<aside class='nav-drawer' id='kingpin-drawer' aria-label='Main navigation' aria-hidden='true' inert>"
        "<div class='drawer-header'><div><span class='drawer-eyebrow'>WELCOME TO</span><strong>HOTEL KINGPIN</strong></div>"
        "<button type='button' class='drawer-close' id='drawer-close' aria-label='Close menu'>×</button></div>"
        "<p class='drawer-hint'>Your stay, your way.</p>"
        "<nav class='drawer-links' aria-label='Hotel navigation'>"
        "<a href='/'>⌂ <span>Home</span></a>"
        "<a href='/#rooms'>▦ <span>Rooms &amp; availability</span></a>"
        "<a href='/menu'>♨ <span>Dining menu</span></a>"
        "<a href='/stays'>▤ <span>My stays</span></a>"
        "<a href='/account'>♙ <span>My account</span></a>"
        "<a href='/login'>↪ <span>Sign in</span></a>"
        "</nav><div class='drawer-bottom'>Room service · Freshly prepared · HOTEL KINGPIN</div></aside>"
        "<main>"+body+"</main><footer>HOTEL KINGPIN · Guest services</footer></body></html>";
}
void page(Res& r,string title,string body){r.set_content(shell(title,body),"text/html; charset=utf-8");}
string form(string action){return "<form method='post' action='"+action+"'>";}
string price(int rupees){return "₹"+to_string(rupees);}
int nights(string a,string b){
    auto first=parseDay(a),last=parseDay(b);
    int count=(last-first).count();
    if(first<today() || count<1 || count>30) throw InputError("Choose a future or current check-in and a stay of 1–30 nights.");
    return count;
}
const string occupied="room=? AND arrival<? AND departure>? AND (status='paid' OR (status='hold' AND expires>unixepoch()))";
string searchForm(string a,string b,string type,string floor=""){
    string floors="<option value=''>All floors</option>";
    for(int i=1;i<=4;++i){
        string n=to_string(i);
        floors+="<option value='"+n+"'"+(n==floor?" selected":"")+">Floor "+n+"</option>";
    }
    return "<section id='rooms'><p class='eyebrow'>ROOM RESERVATIONS</p><h2>Find your room</h2><form class='search' action='/rooms'><label>Check-in<input required type='date' name='a' min='"+dateString(today())+"' value='"+a+"'></label><label>Check-out<input required type='date' name='b' value='"+b+"'></label><label>Room preference<select name='type'><option>Any</option><option "+(type=="AC"?"selected":"")+">AC</option><option "+(type=="Non-AC"?"selected":"")+">Non-AC</option></select></label><label>Floor<select name='floor'>"+floors+"</select></label><button>Check availability</button></form></section>";
}
string intro(){return "<div class='hero'><div><p class='eyebrow'>HOTEL KINGPIN</p><h1>Stay at<br>HOTEL KINGPIN.</h1><p>Clear room choices on every floor, updated pricing, and room service delivered right to your door.</p><a class='button light' href='#rooms'>Explore rooms ↓</a></div></div>";}
string roomImage(const Room& room){
    int n = room.number();
    if(room.floor()==1) return "floor1-nonac-single.png";
    if(n>=201 && n<=203) return "floor2-nonac-single.png";
    if(n>=204 && n<=206) return "floor2-ac-double.png";
    if(room.floor()==3) return "floor3-ac-single.png";
    return "floor4-ac-double.png";
}
string roomCard(const Room& room,string info,string action){
    string guests = room.bed()=="Double bed" ? "Best for 2 guests" : "Best for 1 guest";
    return "<article><img alt='"+room.type()+" "+room.bed()+" room' src='/static/photos/"+roomImage(room)+"'><div><p class='eyebrow'>"+"Room "+to_string(room.number())+" · Floor "+to_string(room.floor())+" · "+room.type()+"</p><h3>"+room.bed()+"</h3><p>"+price(room.nightlyRate())+" / night</p><p>"+guests+"</p>"+info+action+"</div></article>";
}
string menu(Store& db,string owner,string category){
    // Category filtering is handled entirely by C++; no client-side JavaScript needed.
    if(category!="breakfast" && category!="lunch" && category!="snacks" && category!="dinner")
        category="breakfast";
    string body="<div class='dining'><p class='eyebrow'>HOTEL KINGPIN · ROOM SERVICE</p><h1>Fresh from our kitchen.</h1>"
                "<p>Breakfast, lunch, snacks and dinner — all in one place.</p></div>"
                "<div class='menu-intro'><div><p class='eyebrow'>EXPLORE THE MENU</p><h2>What would you like to eat?</h2>"
                "<p>Choose your favourites and order to your booked room. Delivery slots follow Indian Standard Time (IST).</p></div>"
                "<div class='menu-summary'><strong>"+to_string(hotel.dishes.size())+"</strong><span>dishes and drinks</span></div></div>"
                "<nav class='menu-categories' aria-label='Food categories'>"
                "<a href='/menu?category=breakfast'>☀ Breakfast</a><a href='/menu?category=lunch'>◉ Lunch</a>"
                "<a href='/menu?category=snacks'>☕ Snacks</a><a href='/menu?category=dinner'>☾ Dinner</a></nav>";
    auto stays=db.query("SELECT id,room,arrival,departure FROM bookings WHERE owner=? AND status='paid' AND departure>? ORDER BY arrival",{owner,dateString(today())});
    if(stays.empty()){
        body+="<div class='menu-notice'><div><strong>Feel free to explore every dish!</strong>"
              "<p>To place a room-service order, first confirm a room booking. The menu is always available to browse.</p></div>"
              "<a class='button' href='/#rooms'>Find a room</a></div>";
    }else{
        body+="<div class='menu-notice menu-notice-ready'><div><strong>Room service is ready!</strong>"
              "<p>Choose your booked room, delivery date and quantity below. Food is payable on delivery.</p></div>"
              "<a class='button' href='/stays'>My stays</a></div>";
    }
    const std::vector<string> ids{"breakfast","lunch","snacks","dinner"};
    const std::vector<string> icons{"☀","◉","☕","☾"};
    for(size_t s=0;s<hotel.slots.size();s++){
        if(ids[s]!=category) continue; // Only render dishes from the selected meal.
        const auto& slot=hotel.slots[s];
        body+="<section class='food-section' id='"+ids[s]+"'><div class='food-section-top'>"
              "<div><p class='eyebrow'>"+icons[s]+" · "+slot.hours+" IST</p>"
              "<h2>"+escapeHtml(slot.name)+"</h2></div><span class='food-timing'>"+slot.hours+"</span></div>"
              "<div class='food-grid'>";
        for(size_t i=0;i<hotel.dishes.size();i++){
            const auto& d=hotel.dishes[i];
            if(d.slot!=int(s))continue;
            const string vegClass=d.veg?"veg":"nonveg";
            const string unitText=d.unit=="piece"?" / piece":" / "+d.unit;
            body+="<article class='food-card'><div class='food-meta'><span class='food-dot "+vegClass+"' aria-label='"+
                  (d.veg?string("Vegetarian"):string("Non-vegetarian"))+"'></span><span>"+
                  (d.veg?string("VEG"):string("NON-VEG"))+"</span></div>"
                  "<h3>"+escapeHtml(d.name)+"</h3>"
                  "<p class='food-description'>"+escapeHtml(d.description)+"</p>"
                  "<div class='food-price'>"+price(d.price)+"<span>"+unitText+"</span></div>";
            if(!stays.empty()){
                body+=form("/order")+hidden("dish",to_string(i))+"<div class='food-order-fields'>"
                      "<label>Deliver to<select name='stay'>";
                for(const auto& row:stays){
                    body+="<option value='"+row[0]+"'>Room "+row[1]+" · "+row[2]+" to "+row[3]+"</option>";
                }
                body+="</select></label>"
                      "<label>Delivery date<input type='date' name='day' min='"+dateString(today())+
                      "' value='"+std::max(stays[0][2],dateString(today()))+"' required></label>"
                      "<label>"+string(d.unit=="piece"?"Number of pieces":"Quantity")+"<input type='number' min='1' max='10' value='1' name='qty' required></label>"
                      "</div><button type='submit' class='food-order-button'>Order to room</button></form>";
            }else{
                body+="<p class='food-book-prompt'>Book a room to order</p>";
            }
            body+="</article>";
        }
        body+="</div></section>";
    }
    body+="<p class='menu-footnote'>Prices shown are sample HOTEL KINGPIN menu rates. Payments for food are collected on delivery in this demo.</p>";
    return body;
}
int main(){
    std::filesystem::create_directories("data");
    Store init;
    init.query("CREATE TABLE IF NOT EXISTS bookings(id INTEGER PRIMARY KEY,owner TEXT NOT NULL, room INTEGER NOT NULL,arrival TEXT NOT NULL,departure TEXT NOT NULL,total INTEGER NOT NULL,status TEXT NOT NULL,expires INTEGER NOT NULL)");
    init.query("CREATE TABLE IF NOT EXISTS orders(id INTEGER PRIMARY KEY, booking INTEGER NOT NULL,dish TEXT NOT NULL,qty INTEGER NOT NULL,total INTEGER NOT NULL,day TEXT NOT NULL,slot TEXT NOT NULL)");
    init.query("CREATE TABLE IF NOT EXISTS accounts(id INTEGER PRIMARY KEY, name TEXT NOT NULL, email TEXT NOT NULL UNIQUE COLLATE NOCASE, phone TEXT NOT NULL UNIQUE, salt TEXT NOT NULL,password_hash TEXT NOT NULL,created INTEGER NOT NULL DEFAULT(unixepoch()))");
    init.query("CREATE TABLE IF NOT EXISTS auth_sessions(token TEXT PRIMARY KEY,account_id INTEGER NOT NULL,expires INTEGER NOT NULL)");
    init.query("CREATE TABLE IF NOT EXISTS login_throttle(id TEXT PRIMARY KEY,fails INTEGER NOT NULL,first_at INTEGER NOT NULL)");
    init.query("DELETE FROM auth_sessions WHERE expires<=unixepoch()");
    httplib::Server app;
    app.new_task_queue = [] { return new httplib::ThreadPool(4); };
    app.set_mount_point("/static","public");
    app.set_exception_handler([](const Req&,Res& r,std::exception_ptr ep){
        try{std::rethrow_exception(ep);}catch(const InputError& e){r.status=400;page(r,"Please check","<h2>"+string(e.what())+"</h2><p><a href='/'>Find a room</a> · <a href='/menu'>Return to dining</a> · <a href='/stays'>My stays</a></p>");}
        catch(const std::exception& e){std::cerr<<e.what()<<'\n';r.status=500;page(r,"Service error","<h2>Please try again shortly.</h2>");}
    });
    app.Get("/login",[](const Req& q,Res& r){
        if(!session(q).empty()){r.set_redirect("/",303);return;}
        page(r,"Sign in",authPage(false));
    });
    app.Post("/login",[](const Req& q,Res& r){
        string identity=lower(trim(param(q,"identity")));
        if(identity.find('@')==string::npos)identity=normalizedPhone(identity);
        string password=param(q,"password");
        if(identity.empty() || identity.size()>254 || password.empty() || password.size()>128)
            throw InputError("Please enter a valid email/mobile and password.");
        Store db;
        auto attempts=db.query("SELECT fails FROM login_throttle WHERE id=? AND first_at>unixepoch()-900",{identity});
        if(!attempts.empty() && std::stoi(attempts[0][0])>=6){
            r.status=429;page(r,"Sign in",authPage(false,"Too many attempts. Try again after 15 minutes."));return;
        }
        auto users=db.query("SELECT id,salt,password_hash FROM accounts WHERE email=? OR phone=?",{identity,identity});
        bool correct=false;
        if(!users.empty())correct=passwordMatches(password,users[0][1],users[0][2]);
        if(!correct){
            db.query("INSERT INTO login_throttle(id,fails,first_at) VALUES(?,1,unixepoch()) "
                     "ON CONFLICT(id) DO UPDATE SET "
                     "fails=CASE WHEN first_at<unixepoch()-900 THEN 1 ELSE fails+1 END," 
                     "first_at=CASE WHEN first_at<unixepoch()-900 THEN unixepoch() ELSE first_at END",{identity});
            r.status=401;page(r,"Sign in",authPage(false,"Invalid email/mobile or password."));return;
        }
        db.query("DELETE FROM login_throttle WHERE id=?",{identity});
        setLoginCookie(r,newLoginSession(db,users[0][0]));
        r.set_redirect("/",303);
    });
    app.Get("/register",[](const Req& q,Res& r){
        if(!session(q).empty()){r.set_redirect("/",303);return;}
        page(r,"Register",authPage(true));
    });
    app.Post("/register",[](const Req& q,Res& r){
        string name=trim(param(q,"name"));
        string email=lower(trim(param(q,"email")));
        string phone=normalizedPhone(param(q,"phone"));
        string password=param(q,"password"),confirm=param(q,"confirm");
        if(name.size()<2||name.size()>80||!std::regex_match(email,std::regex(R"(^[a-z0-9._%+\-]+@[a-z0-9.\-]+\.[a-z]{2,}$)"))||email.size()>254||
           !std::regex_match(phone,std::regex("[6-9][0-9]{9}"))||password.size()<10||password.size()>128||password!=confirm){
            r.status=400;page(r,"Register",authPage(true,"Please check your details. Use a 10-digit Indian mobile number and matching passwords of at least 10 characters."));return;
        }
        Store db;
        if(!db.query("SELECT id FROM accounts WHERE email=? OR phone=?",{email,phone}).empty()){
            r.status=409;page(r,"Register",authPage(true,"An account with this email or mobile already exists. Please sign in."));return;
        }
        string salt=secureRandomHex(16);
        db.query("INSERT INTO accounts(name,email,phone,salt,password_hash) VALUES(?,?,?,?,?)",
                 {name,email,phone,salt,passwordHash(password,salt)});
        string accountId=db.query("SELECT id FROM accounts WHERE email=?",{email})[0][0];
        setLoginCookie(r,newLoginSession(db,accountId));
        r.set_redirect("/",303);
    });
    app.Get("/account",[](const Req& q,Res& r){
        string owner=session(q);
        if(owner.empty()){r.set_redirect("/login",303);return;}
        Store db;
        auto profile=db.query("SELECT name,email,phone FROM accounts WHERE id=?",{owner.substr(5)});
        if(profile.empty()){r.set_redirect("/login",303);return;}
        auto& p=profile[0];
        page(r,"My account","<section class='panel account-panel'><p class='eyebrow'>YOUR HOTEL KINGPIN ACCOUNT</p><h1>Hello, "+escapeHtml(p[0])+"!</h1>"
             "<p><b>Email:</b> "+escapeHtml(p[1])+"</p><p><b>Mobile:</b> "+escapeHtml(p[2])+"</p>"
             "<p><a class='button' href='/stays'>See my bookings</a></p>"
             "<form action='/logout' method='post'><button class='secondary'>Sign out</button></form></section>");
    });
    app.Post("/logout",[](const Req& q,Res& r){
        string token=rawSessionToken(q);
        if(!token.empty()){
            Store db;db.query("DELETE FROM auth_sessions WHERE token=?",{token});
        }
        r.set_header("Set-Cookie","kingpin_session=; HttpOnly; SameSite=Strict; Path=/; Max-Age=0");
        r.set_redirect("/login",303);
    });
    app.Get("/",[](const Req& q,Res& r){
        if(session(q).empty()){r.set_redirect("/login",303);return;}
        auto a=today();
        page(r,"Welcome",intro()+searchForm(dateString(a),dateString(a+std::chrono::days(1)),"Any")+"<section><p class='eyebrow'>ACCOMMODATION</p><h2>Our rooms</h2><div class='grid'>"+roomCard(hotel.room("101"),"","")+roomCard(hotel.room("204"),"","")+roomCard(hotel.room("301"),"","")+roomCard(hotel.room("401"),"","")+"</div></section>"
            "<section class='home-dining'><p class='eyebrow'>HOTEL KINGPIN DINING</p><h2>Hungry? We have you covered.</h2>"
            "<p>Idli, Poori, Meals, Bagara Rice, Phulka, Chicken Curry, Snacks and more.</p>"
            "<a href='/menu' class='button light'>Explore the full dining menu →</a></section>");
    });
    app.Get("/rooms",[](const Req& q,Res& r){
        if(session(q).empty()){r.set_redirect("/login",303);return;}
        auto a=param(q,"a"),b=param(q,"b"),type=param(q,"type"),floor=param(q,"floor");
        int count=nights(a,b);
        Store db;
        string cards;
        for(auto& room:hotel.rooms){
            if(!floor.empty() && floor!=to_string(room->floor())) continue;
            if(type!="Any" && type!=room->type()) continue;
            string n=to_string(room->number());
            if(!db.query("SELECT 1 FROM bookings WHERE "+occupied,{n,b,a}).empty()) continue;
            cards+=roomCard(*room,"<p>Room "+n+" · "+to_string(count)+" night(s) · <b>"+price(room->quote(count))+" total</b></p>",form("/book")+hidden("room",n)+hidden("a",a)+hidden("b",b)+"<button>Book room "+n+"</button></form>");
        }
        page(r,"Available rooms",searchForm(a,b,type,floor)+"<h2>Available rooms</h2>"+(cards.empty()?"<p>No rooms available. Try another date or room type.</p>":"<div class='grid'>"+cards+"</div>"));
    });
    app.Post("/book",[](const Req& q,Res& r){
        string owner=session(q);
        if(owner.empty()){r.set_redirect("/login",303);return;}
        Store db;
        auto a=param(q,"a"),b=param(q,"b"),n=param(q,"room");
        auto& room=hotel.room(n);
        int total=room.quote(nights(a,b));
        // Bookings belong to the account, not a temporary browser cookie.
        auto result=db.query("INSERT INTO bookings(owner,room,arrival,departure,total,status,expires) SELECT ?,?,?,?,?,'hold',unixepoch()+600 WHERE NOT EXISTS(SELECT 1 FROM bookings WHERE "+occupied+") RETURNING id",{owner,n,a,b,to_string(total),n,b,a});
        if(result.empty()) throw InputError("That room has just been taken. Please choose another room.");
        r.set_redirect("/checkout?id="+result[0][0],303);
    });
    app.Get("/checkout",[](const Req& q,Res& r){
        if(session(q).empty()){r.set_redirect("/login",303);return;}
        Store db;
        auto rows=db.query("SELECT id,room,total,arrival,departure FROM bookings WHERE id=? AND owner=? AND status='hold' AND expires>unixepoch()",{param(q,"id"),session(q)});
        if(rows.empty()) throw InputError("No active checkout. Check My stays or choose another room.");
        auto& x=rows[0];
        const Room& room = hotel.room(x[1]);
        string details = "<div class='room-summary'><h3>Room " + x[1] + "</h3>"
            "<p>Floor " + to_string(room.floor()) + " · " + room.bed() + " · " + room.type() + "</p>"
            "<h4>In your room</h4><ul><li>Wi-Fi</li><li>TV</li><li>Private bathroom</li>"
            "<li>Fresh towels</li><li>Wardrobe</li></ul></div>";
        page(r,"Checkout","<section class='panel'><p class='eyebrow'>DEMO CHECKOUT</p>"
            "<h1>Confirm your stay.</h1>" + details + "<p>" + x[3] + " → " + x[4] +
            "</p><h2>₹" + x[2] + "</h2><p>Room held for 10 minutes. This simulated payment moves no money.</p>" +
            form("/pay") + hidden("id",x[0]) + "<button>Simulate successful payment</button></form>" +
            form("/cancel") + hidden("id",x[0]) + "<button class='secondary'>Cancel checkout</button></form></section>");
    });
    app.Post("/pay",[](const Req& q,Res& r){
        if(session(q).empty()){r.set_redirect("/login",303);return;}
        Store db; Row args{param(q,"id"),session(q)};
        auto result=db.query("UPDATE bookings SET status='paid' WHERE id=? AND owner=? AND status='hold' AND expires>unixepoch() RETURNING id",args);
        if(result.empty() && db.query("SELECT id FROM bookings WHERE id=? AND owner=? AND status='paid'",args).empty()) throw InputError("Your room hold expired or was cancelled.");
        r.set_redirect("/stays",303);
    });
    app.Post("/cancel",[](const Req& q,Res& r){
        if(session(q).empty()){r.set_redirect("/login",303);return;}
        Store db;db.query("UPDATE bookings SET status='cancelled' WHERE id=? AND owner=? AND status='hold'",{param(q,"id"),session(q)});
        r.set_redirect("/stays",303);
    });
    app.Get("/menu",[](const Req& q,Res& r){
        Store db;
        string category=q.has_param("category") ? param(q,"category") : "breakfast";
        page(r,"Room service",menu(db,session(q),category));
    });
    app.Post("/order",[](const Req& q,Res& r){
        if(session(q).empty()){r.set_redirect("/login",303);return;}
        auto idx=param(q,"dish"),qty=param(q,"qty"),day=param(q,"day"),id=param(q,"stay");
        if(!std::regex_match(idx,std::regex("[0-9]{1,3}")) || !std::regex_match(qty,std::regex("([1-9]|10)")))
            throw InputError("Choose a listed dish and 1–10 portions or pieces.");
        size_t dishIndex=static_cast<size_t>(std::stoul(idx));
        if(dishIndex>=hotel.dishes.size()) throw InputError("Choose a listed dish.");
        const auto& d=hotel.dishes.at(dishIndex);
        auto& slot=hotel.slots.at(d.slot);
        if(parseDay(day)+std::chrono::hours(slot.end)<=hotelTime()) throw InputError("This delivery slot has ended. Choose a later slot.");
        Store db;
        auto rows=db.query("INSERT INTO orders(booking,dish,qty,total,day,slot) SELECT id,?,?,?,?,? FROM bookings WHERE id=? AND owner=? AND status='paid' AND arrival<=? AND departure>? RETURNING id",{d.name,qty,to_string(d.price*std::stoi(qty)),day,slot.name,id,session(q),day,day});
        if(rows.empty()) throw InputError("Choose a delivery date within your confirmed stay, before check-out.");
        r.set_redirect("/stays",303);
    });
    app.Get("/stays",[](const Req& q,Res& r){
        if(session(q).empty()){r.set_redirect("/login",303);return;}
        Store db; string body="<p class='eyebrow'>GUEST DESK</p><h1>My stays.</h1>";
        auto rows=db.query("SELECT id,room,arrival,departure,total,status FROM bookings WHERE owner=? AND (status='paid' OR (status='hold' AND expires>unixepoch())) ORDER BY id DESC",{session(q)});
        if(rows.empty()) body+="<p>No active reservations in this browser.</p><a class='button' href='/#rooms'>Find a room</a>";
        for(auto& x:rows){
            body+="<section class='panel'><p class='eyebrow'>RESERVATION #"+x[0]+"</p><h2>Room "+x[1]+"</h2><p>"+x[2]+" → "+x[3]+" · ₹"+x[4]+"</p>";
            if(x[5]=="hold") body+="<a href='/checkout?id="+x[0]+"'>Continue checkout →</a>";
            else {
                body+="<p>Booking confirmed · Demo payment received</p><a href='/menu'>Open room-service menu →</a>";
                for(auto& o:db.query("SELECT dish,qty,total,day,slot FROM orders WHERE booking=? ORDER BY id DESC",{x[0]}))
                    body+="<p class='receipt'>"+o[3]+" · "+o[4]+" · "+o[1]+" × "+o[0]+" · ₹"+o[2]+" due on delivery</p>";
            }
            body+="</section>";
        }
        page(r,"My stays",body);
    });
    const char* configuredPort=std::getenv("KINGPIN_BIND_PORT");
    int port=configuredPort?std::stoi(configuredPort):8080;
    if(port<1||port>65535)throw std::runtime_error("Invalid bind port");
    std::cout<<"HOTEL KINGPIN listening on port "<<port<<std::endl;
    return app.listen("0.0.0.0",port)?0:1;
}
