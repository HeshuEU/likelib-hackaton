#pragma once

#include "base/bytes.hpp"
#include "base/directory.hpp"

#include <leveldb/db.h>

#include <memory>

namespace base
{

class Database
{
  public:
    explicit Database() = default;
    explicit Database(Directory const& path);
    Database(Database&&) = default;
    Database& operator=(Database&&) = default;
    ~Database() = default;
    //=====================
    void open(Directory const& path);
    //=====================
    void get(const Bytes& key, Bytes& res) const;
    bool exists(const Bytes& key) const;
    void put(const Bytes& key, const Bytes& value);
    void remove(const Bytes& key);
    //=====================
  private:
    //======================
    bool inited{false};
    std::unique_ptr<leveldb::DB> _data_base;
    leveldb::ReadOptions _read_options;
    leveldb::WriteOptions _write_options;
    //=====================
    static leveldb::ReadOptions defaultReadOptions();
    static leveldb::WriteOptions defaultWriteOptions();
    static leveldb::Options defaultDBOptions();
    //=====================
};

Database createDefaultDatabaseInstance(Directory const& path);

Database createClearDatabaseInstance(Directory const& path);

} // namespace base