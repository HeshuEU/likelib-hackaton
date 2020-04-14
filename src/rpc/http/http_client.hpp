#pragma once

#include <rpc/base_rpc.hpp>

#include <string>

namespace rpc::http
{

/// Class implementing connect to node by gRPC and call methods
class NodeClient final : BaseRpc
{
  public:
    explicit NodeClient(const std::string& connect_address);

    ~NodeClient() override = default;

    OperationStatus test(uint32_t api_version) override;

    lk::Balance balance(const lk::Address& address) override;

    Info info() override;

    lk::Block get_block(const base::Sha256& block_hash) override;

    std::tuple<OperationStatus, lk::Address, lk::Balance> transaction_create_contract(
      lk::Balance amount,
      const lk::Address& from_address,
      const base::Time& transaction_time,
      lk::Balance gas,
      const std::string& contract_code,
      const std::string& init,
      const lk::Sign& signature) override;

    std::tuple<OperationStatus, std::string, lk::Balance> transaction_message_call(lk::Balance amount,
                                                                                   const lk::Address& from_address,
                                                                                   const lk::Address& to_address,
                                                                                   const base::Time& transaction_time,
                                                                                   lk::Balance fee,
                                                                                   const std::string& data,
                                                                                   const lk::Sign& signature) override;

  private:
};

} // namespace rpc
