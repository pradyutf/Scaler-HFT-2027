#include <bits/stdc++.h>
using namespace std;

// Single-file implementation of an in-memory Limit Order Book (C++17)
// Interfaces required by the assignment are implemented in class OrderBook.
// This file also contains a small main() with example usage and lightweight tests.

struct Order {
    uint64_t order_id;     // Unique order identifier
    bool is_buy;           // true = buy, false = sell
    double price;          // Limit price
    uint64_t quantity;     // Remaining quantity
    uint64_t timestamp_ns; // Order entry timestamp in nanoseconds
};

struct PriceLevel {
    double price;
    uint64_t total_quantity;
};

class OrderBook {
public:
    OrderBook() = default;

    // Insert a new order into the book
    void add_order(const Order& order);

    // Cancel an existing order by its ID
    bool cancel_order(uint64_t order_id);

    // Amend an existing order's price or quantity
    bool amend_order(uint64_t order_id, double new_price, uint64_t new_quantity);

    // Get a snapshot of top N bid and ask levels (aggregated quantities)
    void get_snapshot(size_t depth, std::vector<PriceLevel>& bids, std::vector<PriceLevel>& asks) const;

    // Print current state of the order book
    void print_book(size_t depth = 10) const;

private:
    // Internal representation of a price level: list of Orders (FIFO) + aggregated qty
    struct PriceLevelNode {
        double price;
        uint64_t total_quantity = 0;
        std::list<Order> orders; // maintains FIFO among orders at this price
    };

    // Bid map: highest price first. Ask map: lowest price first.
    std::map<double, PriceLevelNode, std::greater<double>> bids_;
    std::map<double, PriceLevelNode> asks_;

    // Lookup: order_id -> (pointer/it to price-level map, iterator to order inside that level)
    // We'll use std::variant to store either bids_ iterator or asks_ iterator. But simpler: store a bool is_buy
    struct LookupValue {
        bool is_buy;
        // iterator to the appropriate map
        // We store as void* to avoid variant complexities in the map type; cast back when used.
        void* map_iterator_ptr = nullptr;
        std::list<Order>::iterator order_it;
    };

    std::unordered_map<uint64_t, LookupValue> order_lookup_;

    // Helpers to get references from stored map_iterator_ptr
    const PriceLevelNode* map_node_ptr_from_void(void* p) const {
        return static_cast<PriceLevelNode*>(p);
    }

    PriceLevelNode* map_node_ptr_from_void(void* p) {
        return static_cast<PriceLevelNode*>(p);
    }

    // Because storing raw pointer to map value is invalid if map rebalances/moves values on insert/erase,
    // we instead will store the map iterator itself inside the LookupValue by allocating a small wrapper object
    // on the heap and pointing to it. Heap allocation per order lookup is minimal; for a true pool you can
    // optimize this (bonus). For clarity and safety we keep this approach.

    struct MapIterHolderBase {
        virtual ~MapIterHolderBase() = default;
    };
    template<typename MapIter>
    struct MapIterHolder : MapIterHolderBase {
        MapIter it;
        MapIterHolder(const MapIter& i) : it(i) {}
    };

    std::unordered_map<uint64_t, std::unique_ptr<MapIterHolderBase>> iter_holders_;

    template<typename Map>
    void store_lookup_map_iter(uint64_t order_id, const typename Map::iterator& it) {
        using Holder = MapIterHolder<typename Map::iterator>;
        iter_holders_[order_id] = std::make_unique<Holder>(it);
        // store pointer to the holder object in lookup so we can cast back later
        order_lookup_[order_id].map_iterator_ptr = iter_holders_[order_id].get();
    }

    template<typename Map>
    typename Map::const_iterator get_map_iter_const(bool is_buy, void* holder_ptr) const {
        using Holder = MapIterHolder<typename Map::const_iterator>;
        auto base = static_cast<MapIterHolder<typename Map::iterator>*>(holder_ptr);
        return base->it;
    }

    template<typename Map>
    typename Map::iterator get_map_iter(bool is_buy, void* holder_ptr) {
        auto base = static_cast<MapIterHolder<typename Map::iterator>*>(holder_ptr);
        return base->it;
    }
};

// Implementation

void OrderBook::add_order(const Order& order) {
    if (order.quantity == 0) return; // ignore zero-sized orders

    if (order.is_buy) {
        auto it = bids_.find(order.price);
        if (it == bids_.end()) {
            // create new price level
            PriceLevelNode node;
            node.price = order.price;
            node.total_quantity = order.quantity;
            node.orders.push_back(order);
            auto inserted = bids_.emplace(order.price, std::move(node)).first;

            // store lookup info
            order_lookup_[order.order_id] = {true, nullptr, std::prev(inserted->second.orders.end())};
            store_lookup_map_iter<decltype(bids_)>(order.order_id, inserted);
        } else {
            // append to existing price level
            it->second.orders.push_back(order);
            it->second.total_quantity += order.quantity;

            order_lookup_[order.order_id] = {true, nullptr, std::prev(it->second.orders.end())};
            store_lookup_map_iter<decltype(bids_)>(order.order_id, it);
        }
    } else {
        auto it = asks_.find(order.price);
        if (it == asks_.end()) {
            PriceLevelNode node;
            node.price = order.price;
            node.total_quantity = order.quantity;
            node.orders.push_back(order);
            auto inserted = asks_.emplace(order.price, std::move(node)).first;

            order_lookup_[order.order_id] = {false, nullptr, std::prev(inserted->second.orders.end())};
            store_lookup_map_iter<decltype(asks_)>(order.order_id, inserted);
        } else {
            it->second.orders.push_back(order);
            it->second.total_quantity += order.quantity;

            order_lookup_[order.order_id] = {false, nullptr, std::prev(it->second.orders.end())};
            store_lookup_map_iter<decltype(asks_)>(order.order_id, it);
        }
    }
}

bool OrderBook::cancel_order(uint64_t order_id) {
    auto lookup_it = order_lookup_.find(order_id);
    if (lookup_it == order_lookup_.end()) return false;

    LookupValue lv = lookup_it->second;
    // retrieve map iterator from iter_holders_
    auto holder_it = iter_holders_.find(order_id);
    if (holder_it == iter_holders_.end()) return false; // should not happen

    if (lv.is_buy) {
        auto map_holder = static_cast<MapIterHolder<decltype(bids_)::iterator>*>(holder_it->second.get());
        auto map_it = map_holder->it;
        auto& pl = map_it->second;
        uint64_t qty = lv.order_it->quantity;
        // subtract
        if (pl.total_quantity >= qty) pl.total_quantity -= qty; else pl.total_quantity = 0;
        // erase order from list
        pl.orders.erase(lv.order_it);
        // cleanup
        order_lookup_.erase(lookup_it);
        iter_holders_.erase(order_id);
        // remove price level if empty
        if (pl.orders.empty()) {
            bids_.erase(map_it);
        }
        return true;
    } else {
        auto map_holder = static_cast<MapIterHolder<decltype(asks_)::iterator>*>(holder_it->second.get());
        auto map_it = map_holder->it;
        auto& pl = map_it->second;
        uint64_t qty = lv.order_it->quantity;
        if (pl.total_quantity >= qty) pl.total_quantity -= qty; else pl.total_quantity = 0;
        pl.orders.erase(lv.order_it);
        order_lookup_.erase(lookup_it);
        iter_holders_.erase(order_id);
        if (pl.orders.empty()) {
            asks_.erase(map_it);
        }
        return true;
    }
}

bool OrderBook::amend_order(uint64_t order_id, double new_price, uint64_t new_quantity) {
    auto lookup_it = order_lookup_.find(order_id);
    if (lookup_it == order_lookup_.end()) return false;
    LookupValue lv = lookup_it->second;
    auto holder_it = iter_holders_.find(order_id);
    if (holder_it == iter_holders_.end()) return false;

    if (lv.is_buy) {
        auto map_holder = static_cast<MapIterHolder<decltype(bids_)::iterator>*>(holder_it->second.get());
        auto map_it = map_holder->it;
        auto& pl = map_it->second;
        // If price changed -> cancel + add with new price/qty and timestamp preserved
        if (new_price != pl.price) {
            Order updated = *lv.order_it;
            updated.price = new_price;
            updated.quantity = new_quantity;
            // remove old
            uint64_t old_qty = lv.order_it->quantity;
            if (pl.total_quantity >= old_qty) pl.total_quantity -= old_qty; else pl.total_quantity = 0;
            pl.orders.erase(lv.order_it);
            order_lookup_.erase(lookup_it);
            iter_holders_.erase(holder_it);
            if (pl.orders.empty()) bids_.erase(map_it);
            // add new order
            add_order(updated);
            return true;
        } else {
            // same price -> update quantity in place
            uint64_t old_qty = lv.order_it->quantity;
            if (new_quantity == old_qty) return true; // nothing to do
            lv.order_it->quantity = new_quantity;
            if (new_quantity > old_qty) pl.total_quantity += (new_quantity - old_qty);
            else pl.total_quantity -= (old_qty - new_quantity);
            return true;
        }
    } else {
        auto map_holder = static_cast<MapIterHolder<decltype(asks_)::iterator>*>(holder_it->second.get());
        auto map_it = map_holder->it;
        auto& pl = map_it->second;
        if (new_price != pl.price) {
            Order updated = *lv.order_it;
            updated.price = new_price;
            updated.quantity = new_quantity;
            uint64_t old_qty = lv.order_it->quantity;
            if (pl.total_quantity >= old_qty) pl.total_quantity -= old_qty; else pl.total_quantity = 0;
            pl.orders.erase(lv.order_it);
            order_lookup_.erase(lookup_it);
            iter_holders_.erase(holder_it);
            if (pl.orders.empty()) asks_.erase(map_it);
            add_order(updated);
            return true;
        } else {
            uint64_t old_qty = lv.order_it->quantity;
            if (new_quantity == old_qty) return true;
            lv.order_it->quantity = new_quantity;
            if (new_quantity > old_qty) pl.total_quantity += (new_quantity - old_qty);
            else pl.total_quantity -= (old_qty - new_quantity);
            return true;
        }
    }
}

void OrderBook::get_snapshot(size_t depth, std::vector<PriceLevel>& bids, std::vector<PriceLevel>& asks) const {
    bids.clear(); asks.clear();
    size_t count = 0;
    for (auto it = bids_.begin(); it != bids_.end() && count < depth; ++it, ++count) {
        bids.push_back({it->first, it->second.total_quantity});
    }
    count = 0;
    for (auto it = asks_.begin(); it != asks_.end() && count < depth; ++it, ++count) {
        asks.push_back({it->first, it->second.total_quantity});
    }
}

void OrderBook::print_book(size_t depth) const {
    vector<PriceLevel> bids, asks;
    get_snapshot(depth, bids, asks);

    cout << "------ ORDER BOOK (Top " << depth << " levels) ------\n";
    cout << "   Bids (price x qty)" << string(30, ' ') << "Asks (price x qty)\n";
    size_t rows = max(bids.size(), asks.size());
    for (size_t i = 0; i < rows; ++i) {
        if (i < bids.size()) cout << fixed << setprecision(2) << bids[i].price << " x " << bids[i].total_quantity;
        else cout << string(15, ' ');
        cout << string(20, ' ');
        if (i < asks.size()) cout << fixed << setprecision(2) << asks[i].price << " x " << asks[i].total_quantity;
        cout << '\n';
    }
    cout << "-------------------------------------------\n";
}

// ---------- Example usage and simple smoke tests ----------
#ifdef ORDER_BOOK_DEMO
int main() {
    OrderBook ob;

    uint64_t ts = 0;
    auto mk = [&](uint64_t id, bool buy, double p, uint64_t q) {
        return Order{id, buy, p, q, ts++};
    };

    ob.add_order(mk(1, true, 100.0, 500));
    ob.add_order(mk(2, true, 101.0, 200));
    ob.add_order(mk(3, false, 102.0, 300));
    ob.add_order(mk(4, false, 103.0, 400));
    ob.add_order(mk(5, true, 101.0, 100)); // same level as 2 -> FIFO after it

    cout << "Initial book:\n";
    ob.print_book(5);

    cout << "\nCancel order 2\n";
    ob.cancel_order(2);
    ob.print_book(5);

    cout << "\nAmend order 5 (quantity -> 50)\n";
    ob.amend_order(5, 101.0, 50);
    ob.print_book(5);

    cout << "\nAmend order 1 (price -> 102.0) (moves side)\n";
    ob.amend_order(1, 102.0, 500);
    ob.print_book(5);

    // Snapshot
    vector<PriceLevel> bids, asks;
    ob.get_snapshot(3, bids, asks);
    cout << "Snapshot top 3 bids:\n";
    for (auto &b : bids) cout << b.price << " x " << b.total_quantity << '\n';

    return 0;
}
#endif

