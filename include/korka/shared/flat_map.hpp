#pragma once

#include <vector>

#include <vector>
#include <algorithm>
#include <optional>
#include <utility>

namespace korka {
  template<typename Key, typename Value>
  class flat_map {
  public:
    using value_type = std::pair<Key, Value>;

    constexpr flat_map() = default;

    constexpr explicit flat_map(std::vector<value_type> data)
      : m_data(std::move(data)) {
      sort_data();
    }

    constexpr auto find(const Key &key) const {
      auto it = lower_bound(key);
      if (it != m_data.end() && it->first == key) {
        return it;
      }
      return m_data.end();
    }

    constexpr auto operator[](const Key &key) -> Value & {
      auto it = lower_bound(key);

      if (it == m_data.end() || it->first != key) {
        it = m_data.insert(it, {key, Value{}});
      }

      return it->second;
    }

    constexpr auto contains(const Key &key) const -> bool {
      return find(key) != m_data.end();
    }

    constexpr auto insert(Key key, Value value) -> void {
      auto it = lower_bound(key);
      if (it != m_data.end() && it->first == key) {
        it->second = std::move(value);
      } else {
        m_data.insert(it, {std::move(key), std::move(value)});
      }
    }

    constexpr auto emplace(auto &&...args) -> void {
      auto pair = value_type{std::forward<decltype(args)>(args)...};

      auto it = lower_bound(pair.first);
      if (it != m_data.end() && it->first == pair.first) {
        it->second = std::move(pair.second);
      } else {
        m_data.insert(it, {std::move(pair.first), std::move(pair.second)});
      }
    }

    constexpr auto begin() const { return m_data.begin(); }

    constexpr auto end() const { return m_data.end(); }

    constexpr auto size() const -> std::size_t { return m_data.size(); }

    constexpr auto clear() {
      m_data.clear();
    }

  private:
    std::vector<value_type> m_data;

    constexpr auto lower_bound(const Key &key) const {
      return std::lower_bound(m_data.begin(), m_data.end(), key,
                              [](const value_type &pair, const Key &k) {
                                return pair.first < k;
                              });
    }

    constexpr auto lower_bound(const Key &key) {
      return std::lower_bound(m_data.begin(), m_data.end(), key,
                              [](const value_type &pair, const Key &k) {
                                return pair.first < k;
                              });
    }

    constexpr auto sort_data() -> void {
      std::sort(m_data.begin(), m_data.end(), [](const auto &a, const auto &b) {
        return a.first < b.first;
      });
    }
  };
}