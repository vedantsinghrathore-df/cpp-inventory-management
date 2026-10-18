#pragma once
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace ims {
using Money = std::int64_t; // Integer paise; never floating point.
constexpr Money maxPrice = 1000000000000LL;
constexpr int maxQuantity = 1000000;
struct Product { int id; std::string name; int quantity; Money price; };
struct Transaction {
    std::int64_t sequence;
    std::string timestamp, type;
    int productId;
    std::string productName;
    int quantity; // Added/sold/removed quantity; zero for price changes.
    Money unitPrice;
};
Money parseMoney(const std::string& input);
std::string formatMoney(Money value);
int parseInteger(const std::string& input, int minimum, int maximum);
class Inventory {
public:
    // Empty path selects an in-memory inventory (useful for tests).
    explicit Inventory(std::filesystem::path file = {});
    void add(int id, const std::string& name, int quantity, Money price);
    void remove(int id);
    void restock(int id, int quantity);
    Money sell(int id, int quantity);
    void setPrice(int id, Money price);
    Product get(int id) const;
    std::vector<Product> search(const std::string& query) const;
    std::vector<Product> products() const;
    std::vector<Product> lowStock(int threshold) const;
    const std::vector<Transaction>& transactions() const { return history_; }
    Money totalValue() const;
private:
    std::filesystem::path file_;
    std::map<int, Product> products_;
    std::vector<Transaction> history_;
    void load();
    void persist() const;
    void commit(Inventory next, const std::string& type, const Product& product, int quantity);
};
}
