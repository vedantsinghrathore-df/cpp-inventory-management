#include "inventory.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace ims {
namespace {
std::string trim(const std::string& s) {
    auto a=s.find_first_not_of(" \t\r\n");
    if(a==std::string::npos) return {};
    return s.substr(a,s.find_last_not_of(" \t\r\n")-a+1);
}
std::string lower(std::string s) {
    for(char& c:s) c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
void validPrice(Money p) {
    if(p<0 || p>maxPrice) throw std::invalid_argument("Price must be between 0 and 10000000000.00.");
}
void validProduct(const Product& p) {
    if(p.id<=0) throw std::invalid_argument("Product ID must be positive.");
    if(p.name.empty() || p.name.size()>100 || trim(p.name)!=p.name)
        throw std::invalid_argument("Name must contain 1-100 characters with no outer whitespace.");
    for(unsigned char c:p.name) if(std::iscntrl(c)) throw std::invalid_argument("Name contains control characters.");
    if(p.quantity<0 || p.quantity>maxQuantity) throw std::invalid_argument("Stock must be between 0 and 1000000.");
    validPrice(p.price);
}
std::string now() {
    std::time_t t=std::time(nullptr);
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc,&t);
#else
    gmtime_r(&t,&utc);
#endif
    std::ostringstream out;
    out<<std::put_time(&utc,"%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}
void endOfLine(std::istringstream& line) {
    line>>std::ws;
    if(!line.eof()) throw std::runtime_error("Unexpected data in inventory file.");
}
}
int parseInteger(const std::string& input,int minimum,int maximum) {
    const auto s=trim(input);
    if(s.empty()) throw std::invalid_argument("Enter a whole number.");
    std::int64_t n=0;
    for(char c:s) {
        if(c<'0'||c>'9') throw std::invalid_argument("Enter digits only.");
        if(n>(static_cast<std::int64_t>(maximum)-(c-'0'))/10)
            throw std::invalid_argument("Number outside allowed range.");
        n=n*10+(c-'0');
        if(n>maximum) throw std::invalid_argument("Number outside allowed range.");
    }
    if(n<minimum) throw std::invalid_argument("Number outside allowed range.");
    return static_cast<int>(n);
}
Money parseMoney(const std::string& input) {
    const auto s=trim(input);
    if(s.empty()) throw std::invalid_argument("Enter a price, for example 149.50.");
    const auto dot=s.find('.');
    auto whole=s.substr(0,dot);
    auto fraction=dot==std::string::npos ? std::string{} : s.substr(dot+1);
    if(whole.empty() || (dot!=std::string::npos && (fraction.empty()||fraction.size()>2)))
        throw std::invalid_argument("Use at most two decimal places.");
    Money amount=0;
    for(char c:whole) {
        if(c<'0'||c>'9') throw std::invalid_argument("Invalid price.");
        if(amount>(maxPrice/100-(c-'0'))/10) throw std::invalid_argument("Price too large.");
        amount=amount*10+c-'0';
        if(amount>maxPrice/100) throw std::invalid_argument("Price too large.");
    }
    int cents=0;
    for(char c:fraction) {
        if(c<'0'||c>'9') throw std::invalid_argument("Invalid price.");
        cents=cents*10+c-'0';
    }
    if(fraction.size()==1) cents*=10;
    amount=amount*100+cents;
    validPrice(amount);
    return amount;
}
std::string formatMoney(Money value) {
    std::ostringstream out;
    out<<value/100<<'.'<<std::setw(2)<<std::setfill('0')<<value%100;
    return out.str();
}
Inventory::Inventory(std::filesystem::path file):file_(std::move(file)) { load(); }
Product Inventory::get(int id) const {
    auto it=products_.find(id);
    if(it==products_.end()) throw std::invalid_argument("Product ID not found.");
    return it->second;
}
void Inventory::commit(Inventory next,const std::string& type,const Product& product,int quantity) {
    if(next.history_.size()>=1000000) throw std::runtime_error("Transaction limit reached.");
    next.history_.push_back({static_cast<std::int64_t>(next.history_.size())+1,now(),type,
                            product.id,product.name,quantity,product.price});
    (void)next.totalValue(); // Check aggregate overflow before committing.
    next.persist(); // A failed write leaves the live in-memory state unchanged.
    products_.swap(next.products_);
    history_.swap(next.history_);
}
void Inventory::add(int id,const std::string& name,int quantity,Money price) {
    Product p{id,trim(name),quantity,price}; validProduct(p);
    if(products_.count(id)) throw std::invalid_argument("Product ID already exists.");
    if(products_.size()>=100000) throw std::runtime_error("Product limit reached.");
    auto next=*this; next.products_.emplace(id,p); commit(std::move(next),"ADD",p,quantity);
}
void Inventory::remove(int id) {
    auto p=get(id); auto next=*this; next.products_.erase(id);
    commit(std::move(next),"REMOVE",p,p.quantity);
}
void Inventory::restock(int id,int quantity) {
    auto p=get(id);
    if(quantity<=0 || quantity>maxQuantity-p.quantity) throw std::invalid_argument("Invalid restock quantity or stock limit exceeded.");
    auto next=*this; next.products_.at(id).quantity+=quantity;
    commit(std::move(next),"RESTOCK",p,quantity);
}
Money Inventory::sell(int id,int quantity) {
    auto p=get(id);
    if(quantity<=0 || quantity>p.quantity) throw std::invalid_argument("Sale must be positive and cannot exceed stock.");
    auto next=*this; next.products_.at(id).quantity-=quantity;
    commit(std::move(next),"SALE",p,quantity); return p.price*quantity;
}
void Inventory::setPrice(int id,Money price) {
    auto p=get(id); validPrice(price); p.price=price;
    auto next=*this; next.products_.at(id).price=price;
    commit(std::move(next),"PRICE",p,0);
}
std::vector<Product> Inventory::products() const {
    std::vector<Product> result;
    for(const auto& entry:products_) result.push_back(entry.second);
    return result;
}
std::vector<Product> Inventory::search(const std::string& query) const {
    std::vector<Product> result; const auto q=lower(trim(query));
    for(const auto& p:products()) if(lower(p.name).find(q)!=std::string::npos || std::to_string(p.id)==q) result.push_back(p);
    return result;
}
std::vector<Product> Inventory::lowStock(int threshold) const {
    if(threshold<0 || threshold>maxQuantity) throw std::invalid_argument("Invalid low-stock threshold.");
    std::vector<Product> result;
    for(const auto& p:products()) if(p.quantity<=threshold) result.push_back(p);
    return result;
}
Money Inventory::totalValue() const {
    Money total=0;
    for(const auto& entry:products_) {
        const auto& p=entry.second; Money v=p.price*p.quantity;
        if(total>std::numeric_limits<Money>::max()-v) throw std::overflow_error("Total inventory value is too large.");
        total+=v;
    }
    return total;
}
void Inventory::persist() const {
    if(file_.empty()) return;
    auto temp=file_; temp+=".tmp";
    try {
        std::ofstream out(temp,std::ios::trunc);
        if(!out) throw std::runtime_error("Cannot open temporary inventory file. Check the folder and permissions.");
        out<<"IMS 1\n";
        for(const auto& p:products()) out<<"P "<<p.id<<' '<<std::quoted(p.name)<<' '<<p.quantity<<' '<<p.price<<'\n';
        for(const auto& t:history_) out<<"T "<<t.sequence<<' '<<std::quoted(t.timestamp)<<' '<<t.type<<' '
            <<t.productId<<' '<<std::quoted(t.productName)<<' '<<t.quantity<<' '<<t.unitPrice<<'\n';
        out<<"END "<<products_.size()<<' '<<history_.size()<<'\n';
        out.close();
        if(!out) throw std::runtime_error("Could not finish writing inventory file.");
#ifdef _WIN32
        if(!MoveFileExW(temp.c_str(),file_.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Could not replace inventory file.");
#else
        if(std::rename(temp.c_str(),file_.c_str())!=0) throw std::runtime_error("Could not replace inventory file.");
#endif
    } catch(...) { std::error_code ec; std::filesystem::remove(temp,ec); throw; }
}
void Inventory::load() {
    if(file_.empty() || !std::filesystem::exists(file_)) return;
    std::ifstream in(file_); std::string row;
    if(!in || !std::getline(in,row) || row!="IMS 1") throw std::runtime_error("Invalid inventory file header. Original file was preserved.");
    bool ended=false, transactionsStarted=false;
    while(std::getline(in,row)) {
        if(ended) throw std::runtime_error("Data after inventory end marker.");
        std::istringstream line(row); std::string tag; line>>tag;
        if(tag=="P") {
            Product p{};
            if(transactionsStarted || !(line>>p.id>>std::quoted(p.name)>>p.quantity>>p.price)) throw std::runtime_error("Invalid product record.");
            validProduct(p); endOfLine(line);
            if(!products_.emplace(p.id,p).second || products_.size()>100000) throw std::runtime_error("Duplicate ID or excessive product count.");
        } else if(tag=="T") {
            transactionsStarted=true; Transaction t{};
            if(!(line>>t.sequence>>std::quoted(t.timestamp)>>t.type>>t.productId>>std::quoted(t.productName)>>t.quantity>>t.unitPrice))
                throw std::runtime_error("Invalid transaction record.");
            endOfLine(line);
            validProduct({t.productId,t.productName,t.quantity,t.unitPrice});
            if(t.sequence!=static_cast<std::int64_t>(history_.size())+1 || t.timestamp.size()!=20 || history_.size()>=1000000)
                throw std::runtime_error("Invalid transaction sequence or timestamp.");
            if(t.type!="ADD" && t.type!="REMOVE" && t.type!="RESTOCK" && t.type!="SALE" && t.type!="PRICE")
                throw std::runtime_error("Unknown transaction type.");
            if((t.type=="PRICE" && t.quantity!=0) || ((t.type=="SALE" || t.type=="RESTOCK") && t.quantity==0))
                throw std::runtime_error("Invalid transaction quantity.");
            history_.push_back(t);
        } else if(tag=="END") {
            std::size_t pc=0,tc=0;
            if(!(line>>pc>>tc) || pc!=products_.size() || tc!=history_.size()) throw std::runtime_error("Invalid inventory record counts.");
            endOfLine(line); ended=true;
        } else throw std::runtime_error("Unknown inventory record.");
    }
    if(in.bad() || !ended) throw std::runtime_error("Incomplete inventory file. Original file was preserved.");
    (void)totalValue();
}
}
