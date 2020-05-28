#pragma once

#include "core/block.hpp"
#include "core/blockchain.hpp"
#include "core/host.hpp"
#include "core/managers.hpp"
#include "core/protocol.hpp"

#include "vm/vm.hpp"

#include "base/crypto.hpp"
#include "base/property_tree.hpp"
#include "base/utility.hpp"

#include <shared_mutex>

namespace lk
{

class ViewCall
{
  public:
    ViewCall(lk::Address from,
             lk::Address contract_address,
             base::Time timestamp,
             base::Bytes data,
             lk::Sign sign = lk::Sign{});
    ViewCall(const ViewCall&) = default;
    ViewCall(ViewCall&&) = default;
    ViewCall& operator=(const ViewCall&) = default;
    ViewCall& operator=(ViewCall&&) = default;
    ~ViewCall() = default;
    //=================
    const lk::Address& getFrom() const noexcept;
    const lk::Address& getContractAddress() const noexcept;
    const base::Time& getTimestamp() const noexcept;
    const base::Bytes& getData() const noexcept;
    //=================
    void sign(const base::Secp256PrivateKey& key);
    bool checkSign() const;
    const lk::Sign& getSign() const noexcept;
    //=================
    base::Sha256 hashOfCall() const;

  private:
    lk::Address _from;
    lk::Address _contract_address;
    base::Bytes _data;
    base::Time _timestamp;
    Sign _sign;
};


class EthHost;


class Core
{
    friend EthHost;

  public:
    //==================
    Core(const base::PropertyTree& config, const base::KeyVault& vault);

    /**
     *  @brief Stops network, does cleaning and flushing.
     *
     *  @threadsafe
     */
    ~Core() = default;
    //==================
    /**
     *  @brief Loads blockchain from disk and runs networking.
     *
     *  @async
     *  @threadsafe
     */
    void run();
    //==================
    lk::AccountInfo getAccountInfo(const lk::Address& address) const;
    //==================
    TransactionStatus addPendingTransaction(const lk::Transaction& tx);
    //==================
    std::optional<TransactionStatus> getTransactionOutput(const base::Sha256& tx_hash);
    void addTransactionOutput(const base::Sha256& tx, const TransactionStatus& status);
    //==================
    bool tryAddBlock(const lk::Block& b);
    std::optional<lk::Block> findBlock(const base::Sha256& hash) const;
    std::optional<base::Sha256> findBlockHash(const lk::BlockDepth& depth) const;
    std::optional<lk::Transaction> findTransaction(const base::Sha256& hash) const;
    const lk::Block& getTopBlock() const;
    //==================
    lk::Block getBlockTemplate() const;
    //==================
    const lk::Address& getThisNodeAddress() const noexcept;
    //==================
    base::Bytes callViewMethod(const lk::ViewCall& call);

  private:
    //==================
    const base::PropertyTree& _config;
    const base::KeyVault& _vault;
    const lk::Address _this_node_address;
    //==================
    base::Observable<const lk::Block&> _event_block_added;
    base::Observable<const lk::Transaction&> _event_new_pending_transaction;
    //==================
    StateManager _state_manager;
    lk::Blockchain _blockchain;
    lk::Host _host;
    //==================
    evmc::VM _vm;
    //==================
    lk::TransactionsSet _pending_transactions;
    mutable std::shared_mutex _pending_transactions_mutex;
    //================
    std::unordered_map<base::Sha256, TransactionStatus> _tx_outputs;
    mutable std::shared_mutex _tx_outputs_mutex;
    //==================
    static const lk::Block& getGenesisBlock();
    void applyBlockTransactions(const lk::Block& block);
    //==================
    bool checkBlock(const lk::Block& block) const;
    //==================
    void tryPerformTransaction(const lk::Transaction& tx, const lk::Block& block_where_tx);
    //==================
    evmc::result callInitContractVm(StateManager& state_manager,
                                    const lk::Block& associated_block,
                                    const lk::Transaction& tx,
                                    const lk::Address& contract_address,
                                    const base::Bytes& code);
    evmc::result callContractVm(StateManager& state_manager,
                                const lk::Block& associated_block,
                                const lk::Transaction& tx,
                                const base::Bytes& code,
                                const base::Bytes& message_data);
    evmc::result callContractAtViewModeVm(StateManager& state_manager,
                                          const lk::Block& associated_block,
                                          const lk::Transaction& associated_tx,
                                          const lk::Address& sender_address,
                                          const lk::Address& contract_address,
                                          const base::Bytes& code,
                                          const base::Bytes& message_data);
    evmc::result callVm(StateManager& state_manager,
                        const lk::Block& associated_block,
                        const lk::Transaction& associated_tx,
                        const evmc_message& message,
                        const base::Bytes& code);

  public:
    //==================
    // notifies if new blocks are added: genesis and blocks, that are stored in DB, are not handled by this
    void subscribeToBlockAddition(decltype(_event_block_added)::CallbackType callback);

    // notifies if some transaction was added to set of pending
    void subscribeToNewPendingTransaction(decltype(_event_new_pending_transaction)::CallbackType callback);
    //==================
};


class EthHost : public evmc::Host
{
  public:
    EthHost(lk::Core& core,
            lk::StateManager& state_manager,
            const lk::Block& associated_block,
            const lk::Transaction& associated_tx);

    bool account_exists(const evmc::address& addr) const noexcept override;

    evmc::bytes32 get_storage(const evmc::address& addr, const evmc::bytes32& ethKey) const noexcept override;

    evmc_storage_status set_storage(const evmc::address& addr,
                                    const evmc::bytes32& ekey,
                                    const evmc::bytes32& evalue) noexcept override;

    evmc::uint256be get_balance(const evmc::address& addr) const noexcept override;

    size_t get_code_size(const evmc::address& addr) const noexcept override;

    evmc::bytes32 get_code_hash(const evmc::address& addr) const noexcept override;

    size_t copy_code(const evmc::address& addr,
                     size_t code_offset,
                     uint8_t* buffer_data,
                     size_t buffer_size) const noexcept override;

    void selfdestruct(const evmc::address& eaddr, const evmc::address& ebeneficiary) noexcept override;

    evmc::result call(const evmc_message& msg) noexcept override;

    evmc_tx_context get_tx_context() const noexcept override;

    evmc::bytes32 get_block_hash(int64_t block_number) const noexcept override;

    void emit_log(const evmc::address&, const uint8_t*, size_t, const evmc::bytes32[], size_t) noexcept;

  private:
    lk::Core& _core;
    lk::StateManager& _state_manager;
    const lk::Block& _associated_block;
    const lk::Transaction& _associated_tx;
};

} // namespace core
