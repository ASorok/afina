#ifndef AFINA_STORAGE_SIMPLE_LRU_H
#define AFINA_STORAGE_SIMPLE_LRU_H

#include <map>
#include <memory>
#include <mutex>
#include <string>

#include <afina/Storage.h>

namespace Afina {
namespace Backend {

/**
 * # Map based implementation
 * That is NOT thread safe implementaiton!!
 */
class SimpleLRU : public Afina::Storage {
public:

    SimpleLRU(size_t max_size = 1024) : _max_size(max_size), _cur_size(0), _lru_head(nullptr), _lru_tail(nullptr) {}

    ~SimpleLRU() {
        _lru_index.clear();
        if (_lru_head != nullptr) {
            while (_lru_head->next != nullptr) {
                std::unique_ptr<lru_node> tmp(nullptr);
                tmp.swap(_lru_head->next);
                _lru_head.swap(tmp);
                tmp.reset();
            }
            _lru_head.reset(); // TODO: here was stack overflow.
        }
    }

    // Implements Afina::Storage interface
    bool Put(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool PutIfAbsent(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool Set(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool Delete(const std::string &key) override;

    // Implements Afina::Storage interface
    // Get is no longer const, since according to LRU logic, the element's
    // position has to be refreshed on each Get
    bool Get(const std::string &key, std::string &value) override;


private:

    bool In(const std::string &key) const override;

    // LRU cache node
    using lru_node = struct lru_node {
        std::string key;
        std::string value;
        // std::unique_ptr<lru_node> prev;
        lru_node *prev;
        std::unique_ptr<lru_node> next;
    };

    // Maximum number of bytes could be stored in this cache.
    // i.e all (keys+values) must be less the _max_size
    std::size_t _max_size;

    // Current number of bytes stored in this cache.
    std::size_t _cur_size;

    // Main storage of lru_nodes, elements in this list ordered descending by "freshness": in the head
    // element that wasn't used for longest time.
    //
    // List owns all nodes
    std::unique_ptr<lru_node> _lru_head;
    lru_node *_lru_tail;

    // Index of nodes from list above, allows fast random access to elements by lru_node#key
    std::map<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>, std::less<std::string>>
        _lru_index;

    // Delete node by it's _lru_index iterator
    bool DeleteItLRU(std::map<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>,
                               std::less<std::string>>::iterator todel_it);

    // Delete node by it's reference
    bool DeleteRefLRU(lru_node &todel_ref);

    // Refresh node by it's iterator in _lru_index
    bool RefreshLRU(lru_node &torefresh_ref);

    // Remove LRU-nodes until we get as much as needfree free space
    bool GetFreeLRU(size_t needfree);

    // Put a new element w/o checking for it's existence
    bool PutLRU(const std::string &key, const std::string &value);

    // Set element value by _lru_index iterator
    bool SetLRU(std::map<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>,
                          std::less<std::string>>::iterator toset_it,
                 const std::string &value);
};

} // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_SIMPLE_LRU_H
