#pragma once

#include <core/block.hpp>
#include <core/managers.hpp>
#include <core/transaction.hpp>
#include <core/types.hpp>


namespace rpc
{


struct Info
{
    base::Sha256 top_block_hash;
    uint64_t top_block_number;
    uint32_t api_version;
    std::size_t peers_number;
};


class BaseRpc
{
  public:
    virtual ~BaseRpc() = default;

    virtual lk::AccountInfo getAccount(const lk::Address& address) = 0;

    virtual Info getNodeInfo() = 0;

    virtual lk::Block getBlock(const base::Sha256& block_hash) = 0;

    virtual lk::Block getBlock(uint64_t block_number) = 0;

    virtual lk::Transaction getTransaction(const base::Sha256& transaction_hash) = 0;

    virtual lk::TransactionStatus pushTransaction(const lk::Transaction& transaction) = 0;

    virtual lk::TransactionStatus getTransactionResult(const base::Sha256& transaction_hash) = 0;

    virtual base::Bytes callContractView(const lk::Address& from,
                                         const lk::Address& contract_address,
                                         const base::Bytes& message) = 0;
};

} // namespace rpc
