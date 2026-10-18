#include "inventory.hpp"
#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
using namespace ims;
namespace {
int passed=0,failed=0;
void require(bool ok) { if(!ok) throw std::runtime_error("Check failed"); }
void rejects(const std::function<void()>& action) {
    bool threw=false;
    try { action(); } catch(const std::exception&) { threw=true; }
    require(threw);
}
void test(const char* name,const std::function<void()>& action) {
    try { action(); ++passed; std::cout<<"PASS "<<name<<'\n'; }
    catch(const std::exception& e) { ++failed; std::cerr<<"FAIL "<<name<<": "<<e.what()<<'\n'; }
}
struct Sandbox {
    std::filesystem::path dir;
    Sandbox() {
        auto base=std::filesystem::temp_directory_path();
        auto nonce=std::chrono::steady_clock::now().time_since_epoch().count();
        for(int i=0;i<100;++i) {
            auto candidate=base/("ims-test-"+std::to_string(nonce)+"-"+std::to_string(i));
            if(std::filesystem::create_directory(candidate)) { dir=candidate; return; }
        }
        throw std::runtime_error("Cannot create test sandbox");
    }
    ~Sandbox() { std::error_code ec; std::filesystem::remove_all(dir,ec); }
};
}
int main() {
    Sandbox box;
    test("empty inventory",[]{ Inventory i; require(i.products().empty() && i.totalValue()==0 && i.transactions().empty()); });
    test("add and trim name",[]{ Inventory i; i.add(1,"  Keyboard  ",4,125050); auto p=i.get(1); require(p.name=="Keyboard" && p.quantity==4 && p.price==125050); });
    test("duplicate ID preserves original",[]{ Inventory i; i.add(1,"A",1,100); rejects([&]{i.add(1,"B",2,200);}); require(i.get(1).name=="A" && i.transactions().size()==1); });
    test("invalid IDs",[]{ Inventory i; rejects([&]{i.add(0,"A",1,1);}); rejects([&]{i.add(-1,"A",1,1);}); rejects([&]{i.get(99);}); });
    test("invalid names",[]{ Inventory i; for(const auto& name:{std::string("  "),std::string(101,'x'),std::string("bad\nname")}) rejects([&]{i.add(1,name,1,1);}); });
    test("invalid initial stock",[]{ Inventory i; rejects([&]{i.add(1,"A",-1,1);}); rejects([&]{i.add(1,"A",maxQuantity+1,1);}); });
    test("price bounds",[]{ Inventory i; rejects([&]{i.add(1,"A",1,-1);}); rejects([&]{i.add(1,"A",1,maxPrice+1);}); i.add(1,"A",0,0); i.setPrice(1,maxPrice); require(i.get(1).price==maxPrice); });
    test("case insensitive search and exact ID",[]{ Inventory i; i.add(12,"Wireless Mouse",1,100); i.add(2,"Desk",1,200); require(i.search("MOUSE").size()==1 && i.search("12")[0].id==12 && i.search("absent").empty()); });
    test("sorted product listing",[]{ Inventory i; i.add(9,"B",1,100); i.add(2,"A",1,200); require(i.products()[0].id==2); });
    test("restock and bounds",[]{ Inventory i; i.add(1,"A",1,100); i.restock(1,2); require(i.get(1).quantity==3); rejects([&]{i.restock(1,0);}); rejects([&]{i.restock(1,-1);}); rejects([&]{i.restock(1,maxQuantity);}); require(i.get(1).quantity==3); });
    test("sale total and stock",[]{ Inventory i; i.add(1,"A",5,1999); require(i.sell(1,2)==3998 && i.get(1).quantity==3); require(i.transactions().back().unitPrice==1999); });
    test("oversell and zero sale preserve state",[]{ Inventory i; i.add(1,"A",2,100); rejects([&]{i.sell(1,3);}); rejects([&]{i.sell(1,0);}); rejects([&]{i.sell(1,-1);}); require(i.get(1).quantity==2 && i.transactions().size()==1); });
    test("price updates preserve sale history",[]{ Inventory i; i.add(1,"A",2,100); i.sell(1,1); i.setPrice(1,250); require(i.transactions()[1].unitPrice==100 && i.sell(1,1)==250); });
    test("remove retains history and ID can be reused",[]{ Inventory i; i.add(1,"Old",3,100); i.remove(1); require(i.products().empty() && i.transactions().size()==2); require(i.transactions()[1].quantity==3); i.add(1,"New",0,200); require(i.transactions()[0].productName=="Old"); });
    test("low stock includes boundary",[]{ Inventory i; i.add(1,"A",0,100); i.add(2,"B",5,100); i.add(3,"C",6,100); require(i.lowStock(5).size()==2); rejects([&]{i.lowStock(-1);}); });
    test("total stock value",[]{ Inventory i; i.add(1,"A",3,125); i.add(2,"B",2,250); require(i.totalValue()==875); });
    test("aggregate overflow rejects mutation",[]{ Inventory i; for(int n=1;n<=9;++n) i.add(n,"A",maxQuantity,maxPrice); rejects([&]{i.add(10,"A",maxQuantity,maxPrice);}); require(i.products().size()==9 && i.transactions().size()==9); });
    test("money parser exact paise",[]{ require(parseMoney(" 149.50 ")==14950 && parseMoney("0.1")==10 && parseMoney("2")==200 && parseMoney("10000000000.00")==maxPrice); require(formatMoney(10)=="0.10" && formatMoney(123456)=="1234.56"); });
    test("money parser rejects malformed or excessive prices",[]{ for(const auto& s:{"","-1","nan","1e3","1.001",".5","1.","1,000","1.2.3","10000000000.01","999999999999999999999999999999"}) rejects([&]{parseMoney(s);}); });
    test("whole number parser rejects junk and overflow",[]{ require(parseInteger(" 12 ",0,20)==12); for(const auto& s:{"","1.5","-1","2x","999999999999999999999"}) rejects([&]{parseInteger(s,0,20);}); rejects([]{parseInteger("21",0,20);}); rejects([]{parseInteger("0",1,20);}); });
    test("persistent round trip after every operation",[&]{
        auto file=box.dir/"roundtrip.db";
        { Inventory i(file); i.add(1,"Mouse \"Pro\" \\ USB",5,1999); i.restock(1,2); i.sell(1,3); i.setPrice(1,2200); }
        { Inventory i(file); require(i.get(1).quantity==4 && i.get(1).price==2200 && i.get(1).name=="Mouse \"Pro\" \\ USB"); require(i.transactions().size()==4 && i.transactions()[2].type=="SALE"); i.remove(1); }
        Inventory i(file); require(i.products().empty() && i.transactions().size()==5);
    });
    test("failed save rolls back inventory and history",[&]{
        auto dir=box.dir/"writable"; std::filesystem::create_directory(dir);
        auto file=dir/"state.db"; Inventory i(file); i.add(1,"A",5,100);
        std::filesystem::create_directory(std::filesystem::path(file.string()+".tmp"));
        { std::ofstream child(file.string()+".tmp/blocker"); child<<"x"; }
        rejects([&]{i.sell(1,2);}); require(i.get(1).quantity==5 && i.transactions().size()==1);
        Inventory reloaded(file); require(reloaded.get(1).quantity==5 && reloaded.transactions().size()==1);
    });
    test("missing parent folder reports save error",[&]{ Inventory i(box.dir/"missing"/"state.db"); rejects([&]{i.add(1,"A",1,100);}); require(i.products().empty()); });
    test("malformed files rejected without overwriting",[&]{
        const std::vector<std::string> bad={"", "IMS 2\n", "IMS 1\nP 1 \"A\" 1 100\n", "IMS 1\nP 1 \"A\" -1 100\nEND 1 0\n", "IMS 1\nP 1 \"A\" 1 100\nP 1 \"B\" 1 100\nEND 2 0\n", "IMS 1\nEND 5 0\n", "IMS 1\nEND 0 0\nextra\n", "IMS 1\nT 9 \"2026-01-01T00:00:00Z\" SALE 1 \"A\" 1 100\nEND 0 1\n", "IMS 1\nT 1 \"2026-01-01T00:00:00Z\" BAD 1 \"A\" 1 100\nEND 0 1\n"};
        for(const auto& content:bad) { auto file=box.dir/"bad.db"; {std::ofstream out(file); out<<content;} rejects([&]{Inventory i(file);}); std::ifstream in(file); std::string after((std::istreambuf_iterator<char>(in)),{}); require(after==content); }
    });
    std::cout<<"\n"<<passed<<" passed, "<<failed<<" failed\n";
    return failed==0?0:1;
}
