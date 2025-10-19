#include <bits/stdc++.h>
using namespace std;

struct Order {
    uint64_t order_id;
    bool is_buy;
    double price;
    uint64_t quantity;
    uint64_t timestamp_ns;
};

struct PriceLevel {
    double price;
    uint64_t total_quantity;
};

class OrderBook {
public:
    void add_order(const Order& order);
    bool cancel_order(uint64_t order_id);
    bool amend_order(uint64_t order_id, double new_price, uint64_t new_quantity);
    void get_snapshot(size_t depth, vector<PriceLevel>& bids, vector<PriceLevel>& asks) const;
    void print_book(size_t depth = 10) const;

private:
    struct PriceLevelNode {
        double price;
        uint64_t total_quantity = 0;
        deque<Order> orders;
    };

    unordered_map<double, PriceLevelNode> bid_map, ask_map;
    vector<double> bid_prices, ask_prices;
    unordered_map<uint64_t, pair<bool, pair<double, size_t>>> order_lookup;

    void insert_price_level(bool is_buy, double price);
    void remove_price_level(bool is_buy, double price);
};

void OrderBook::insert_price_level(bool is_buy, double price) {
    auto& prices = is_buy ? bid_prices : ask_prices;
    auto& mp = is_buy ? bid_map : ask_map;
    if (mp.find(price) == mp.end()) {
        mp[price] = {price, 0, {}};
        prices.push_back(price);
        if (is_buy)
            sort(prices.begin(), prices.end(), greater<>());
        else
            sort(prices.begin(), prices.end());
    }
}

void OrderBook::remove_price_level(bool is_buy, double price) {
    auto& prices = is_buy ? bid_prices : ask_prices;
    auto& mp = is_buy ? bid_map : ask_map;
    mp.erase(price);
    prices.erase(remove(prices.begin(), prices.end(), price), prices.end());
}

void OrderBook::add_order(const Order& order) {
    if (order.quantity == 0) return;
    bool is_buy = order.is_buy;
    auto& mp = is_buy ? bid_map : ask_map;
    insert_price_level(is_buy, order.price);
    auto& node = mp[order.price];
    node.orders.push_back(order);
    node.total_quantity += order.quantity;
    order_lookup[order.order_id] = {is_buy, {order.price, node.orders.size() - 1}};
}

bool OrderBook::cancel_order(uint64_t order_id) {
    auto it = order_lookup.find(order_id);
    if (it == order_lookup.end()) return false;
    bool is_buy = it->second.first;
    double price = it->second.second.first;
    size_t idx = it->second.second.second;
    auto& mp = is_buy ? bid_map : ask_map;
    auto& node = mp[price];
    if (idx >= node.orders.size()) return false;
    uint64_t qty = node.orders[idx].quantity;
    node.orders[idx].quantity = 0;
    node.total_quantity -= qty;
    if (all_of(node.orders.begin(), node.orders.end(), [](const Order& o){ return o.quantity == 0; }))
        remove_price_level(is_buy, price);
    order_lookup.erase(it);
    return true;
}

bool OrderBook::amend_order(uint64_t order_id, double new_price, uint64_t new_quantity) {
    auto it = order_lookup.find(order_id);
    if (it == order_lookup.end()) return false;
    bool is_buy = it->second.first;
    double price = it->second.second.first;
    size_t idx = it->second.second.second;
    auto& mp = is_buy ? bid_map : ask_map;
    auto& node = mp[price];
    if (idx >= node.orders.size()) return false;
    auto& ord = node.orders[idx];
    if (new_price != price) {
        cancel_order(order_id);
        Order updated = ord;
        updated.price = new_price;
        updated.quantity = new_quantity;
        add_order(updated);
    } else {
        node.total_quantity -= ord.quantity;
        ord.quantity = new_quantity;
        node.total_quantity += new_quantity;
    }
    return true;
}

void OrderBook::get_snapshot(size_t depth, vector<PriceLevel>& bids, vector<PriceLevel>& asks) const {
    bids.clear(); asks.clear();
    for (size_t i = 0; i < min(depth, bid_prices.size()); ++i)
        bids.push_back({bid_prices[i], bid_map.at(bid_prices[i]).total_quantity});
    for (size_t i = 0; i < min(depth, ask_prices.size()); ++i)
        asks.push_back({ask_prices[i], ask_map.at(ask_prices[i]).total_quantity});
}

void OrderBook::print_book(size_t depth) const {
    vector<PriceLevel> bids, asks;
    get_snapshot(depth, bids, asks);
    cout << "------ ORDER BOOK ------\n";
    size_t rows = max(bids.size(), asks.size());
    for (size_t i = 0; i < rows; ++i) {
        if (i < bids.size()) cout << fixed << setprecision(2) << bids[i].price << " x " << bids[i].total_quantity;
        else cout << string(15, ' ');
        cout << string(20, ' ');
        if (i < asks.size()) cout << fixed << setprecision(2) << asks[i].price << " x " << asks[i].total_quantity;
        cout << '\n';
    }
    cout << "------------------------\n";
}

#ifdef ORDER_BOOK_DEMO
int main() {
    OrderBook ob;
    uint64_t ts = 0;
    auto mk = [&](uint64_t id, bool buy, double p, uint64_t q){ return Order{id,buy,p,q,ts++}; };
    ob.add_order(mk(1,true,100,500));
    ob.add_order(mk(2,true,101,200));
    ob.add_order(mk(3,false,102,300));
    ob.add_order(mk(4,false,103,400));
    ob.print_book();
    ob.cancel_order(2);
    ob.amend_order(1,102,600);
    ob.print_book();
}
#endif