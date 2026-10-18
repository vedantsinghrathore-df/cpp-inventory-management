#include "inventory.hpp"
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace ims;
namespace {
struct EndOfInput {};
std::string ask(const std::string& prompt) {
    std::cout<<prompt; std::string value;
    if(!std::getline(std::cin,value)) throw EndOfInput{};
    return value;
}
int number(const std::string& prompt,int minimum,int maximum) {
    for(;;) try { return parseInteger(ask(prompt),minimum,maximum); }
    catch(const std::exception& e) { std::cout<<"Error: "<<e.what()<<'\n'; }
}
Money price() {
    for(;;) try { return parseMoney(ask("Price (INR, e.g. 149.50): ")); }
    catch(const std::exception& e) { std::cout<<"Error: "<<e.what()<<'\n'; }
}
int id() { return number("Product ID: ",1,std::numeric_limits<int>::max()); }
void show(const std::vector<Product>& products) {
    if(products.empty()) { std::cout<<"No matching products.\n"; return; }
    std::cout<<std::left<<std::setw(12)<<"ID"<<std::setw(43)<<"Name"<<std::right<<std::setw(10)<<"Stock"<<std::setw(18)<<"Price (INR)"<<'\n';
    for(const auto& p:products) std::cout<<std::left<<std::setw(12)<<p.id<<std::setw(43)
        <<(p.name.size()>40 ? p.name.substr(0,37)+"..." : p.name)<<std::right<<std::setw(10)<<p.quantity<<std::setw(18)<<formatMoney(p.price)<<'\n';
}
}
int main(int argc,char** argv) {
    if(argc>2) { std::cerr<<"Usage: inventory [path-to-inventory.db]\n"; return 1; }
    try {
        Inventory inventory(argc==2?argv[1]:"inventory.db");
        std::cout<<"INVENTORY MANAGEMENT SYSTEM | Currency: INR\nChanges save automatically.\n";
        for(;;) {
            std::cout<<"\n1 List products     2 Add product       3 Search\n"
                       "4 Restock           5 Record sale       6 Update price\n"
                       "7 Remove product    8 Transactions      9 Low stock\n"
                       "10 Inventory value  0 Exit\n";
            try {
                int choice=number("Choose: ",0,10);
                if(choice==0) return 0;
                if(choice==1) show(inventory.products());
                else if(choice==2) {
                    int productId=id(); auto name=ask("Product name: ");
                    int quantity=number("Initial stock: ",0,maxQuantity); auto unitPrice=price();
                    inventory.add(productId,name,quantity,unitPrice); std::cout<<"Product added and saved.\n";
                } else if(choice==3) show(inventory.search(ask("Name fragment or exact ID: ")));
                else if(choice==4) {
                    int productId=id(); int quantity=number("Units to add: ",1,maxQuantity);
                    inventory.restock(productId,quantity); std::cout<<"Stock updated and saved.\n";
                } else if(choice==5) {
                    int productId=id(); int quantity=number("Units sold: ",1,maxQuantity);
                    auto total=inventory.sell(productId,quantity);
                    std::cout<<"Sale saved. Total: INR "<<formatMoney(total)<<'\n';
                } else if(choice==6) {
                    int productId=id(); auto unitPrice=price(); inventory.setPrice(productId,unitPrice);
                    std::cout<<"Price updated and saved.\n";
                } else if(choice==7) {
                    int productId=id(); show({inventory.get(productId)});
                    if(ask("Remove this product and all remaining stock? Type YES: ")=="YES") {
                        inventory.remove(productId); std::cout<<"Product removed; transaction history retained.\n";
                    } else std::cout<<"Cancelled.\n";
                } else if(choice==8) {
                    if(inventory.transactions().empty()) std::cout<<"No transactions yet.\n";
                    for(const auto& t:inventory.transactions())
                        std::cout<<'#'<<t.sequence<<' '<<t.timestamp<<' '<<t.type<<" | "<<t.productId<<" | "
                                 <<t.productName<<" | units "<<t.quantity<<" | unit INR "<<formatMoney(t.unitPrice)<<'\n';
                } else if(choice==9) show(inventory.lowStock(number("Show stock at or below: ",0,maxQuantity)));
                else std::cout<<"Stock value: INR "<<formatMoney(inventory.totalValue())<<'\n';
            } catch(const std::exception& e) { std::cout<<"Error: "<<e.what()<<'\n'; }
        }
    } catch(const EndOfInput&) { std::cout<<"\nInput closed. Saved changes are retained.\n"; return 0; }
    catch(const std::exception& e) { std::cerr<<"Startup failed: "<<e.what()<<'\n'; return 1; }
}
