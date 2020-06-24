#include "public_service.hpp"

#include "core/transaction.hpp"

#include "base/config.hpp"
#include "base/hash.hpp"
#include "base/log.hpp"

void TaskQueue::push(Task&& task)
{
    {
        std::lock_guard lock(_rw_mutex);
        _tasks.emplace_back(std::move(task));
    }
    _has_task.notify_one();
}


Task TaskQueue::get()
{
    std::lock_guard lock(_rw_mutex);
    auto current_task = _tasks.front();
    _tasks.pop_front();
    return current_task;
}


void TaskQueue::wait()
{
    std::unique_lock lock(_rw_mutex);
    _has_task.wait(lock, [this]() { return !_tasks.empty(); });
}


bool TaskQueue::empty() const
{
    std::lock_guard lk(_rw_mutex);
    return _tasks.empty();
}


RpcService::RpcService(const base::PropertyTree& config, lk::Core& core)
  : _config{ config }
  , _core{ core }
  , _server{ config }
{}


RpcService::~RpcService()
{
    stop();
}


void RpcService::run()
{
    _server.run([this](base::PropertyTree call, web_socket::ResponceCall response_callback) {
        if (response_callback) {
            return register_query(std::move(call), std::move(response_callback));
        }
        RAISE_ERROR(base::LogicError, "callback does not set");
    });
    _worker = std::thread(&RpcService::task_worker, this);
}


void RpcService::stop()
{
    if (_worker.joinable()) {
        _worker.join();
    }
}


void RpcService::register_query(base::PropertyTree call, web_socket::ResponceCall responce_callback)
{
    LOG_INFO << "register_query" << call.toString();

    _tasks.push([this, query = std::move(call), callback = std::move(responce_callback)]() {
        try {
            callback(do_route(query));
        }
        catch (const base::Error& er) {
            LOG_ERROR << "error at task" << er;
        }
    });

    LOG_INFO << "task pushed";
}


base::PropertyTree RpcService::do_route(base::PropertyTree call)
{
    auto command_name = call.get<std::string>("name");
    auto command_type = call.get<std::string>("type");
    auto api_version = call.get<std::uint32_t>("api");
    auto id = call.get<std::uint64_t>("id");
    auto args = call.getSubTree("args");

    if (command_name == "account_info") {
        auto address = web_socket::deserializeAddress(args.get<std::string>("address"));
        if (!address) {
            LOG_ERROR << "deserialization error";
        }
        auto info = getAccountInfo(address.value());
        return web_socket::serializeAccountInfo(info);
    }

    if (command_name == "find_block") {
        std::optional<lk::Block> block;
        if (args.hasKey("hash")) {
            auto block_hash = web_socket::deserializeHash(args.get<std::string>("hash"));
            if (!block_hash) {
                LOG_ERROR << "deserialization error";
            }
            block = getBlock(block_hash.value());
        }
        else if (args.hasKey("number")) {
            auto block_number = args.get<std::uint64_t>("number");
            block = getBlock(block_number);
        }
        return web_socket::serializeBlock(block.value());
    }

    if (command_name == "find_transaction") {
        auto tx_hash = web_socket::deserializeHash(args.get<std::string>("hash"));
        if (!tx_hash) {
            LOG_ERROR << "deserialization error";
        }
        auto tx = getTransaction(tx_hash.value());
        return web_socket::serializeTransaction(tx);
    }

    if (command_name == "find_transaction_status") {
        auto tx_hash = web_socket::deserializeHash(args.get<std::string>("hash"));
        if (!tx_hash) {
            LOG_ERROR << "deserialization error";
        }
        auto tx_status = getTransactionStatus(tx_hash.value());
        return web_socket::serializeTransactionStatus(tx_status);
    }

    if (command_name == "last_block_info") {
        LOG_INFO << "last_block_info" << call.toString();
        auto info = getNodeInfo();
        return web_socket::serializeInfo(info);
    }

    if (command_name == "find_block") {
        auto tx_hash = web_socket::deserializeHash(args.get<std::string>("hash"));
        if (!tx_hash) {
            LOG_ERROR << "deserialization error";
        }
        auto tx = getTransaction(tx_hash.value());
        return web_socket::serializeTransaction(tx);
    }

    if (command_name == "call_contract_view") {
        auto view_call = web_socket::deserializeViewCall(args);
        if (!view_call) {
            LOG_ERROR << "deserialization error";
        }
        auto result = callContractView(view_call.value());
        base::PropertyTree answer;
        answer.add("result", web_socket::serializeBytes(result));
        return answer;
    }

    if (command_name == "push_transaction") {
        auto tx = web_socket::deserializeTransaction(args);
        if (!tx) {
            LOG_ERROR << "deserialization error";
        }
        auto status = pushTransaction(tx.value());
        return web_socket::serializeTransactionStatus(status);
    }

    ASSERT_SOFT(false);
}


[[noreturn]] void RpcService::task_worker() noexcept
{
    while (true) {
        try {

            LOG_INFO << "try task";
            _tasks.wait();
            auto task = _tasks.get();
            task();
            LOG_INFO << "executed task";
        }
        catch (...) {
            LOG_ERROR << "error at task execution";
        }
    }
}


lk::AccountInfo RpcService::getAccountInfo(const lk::Address& address)
{
    LOG_TRACE << "Received RPC request {getAccount}" << address;
    return _core.getAccountInfo(address);
}


web_socket::NodeInfo RpcService::getNodeInfo()
{
    LOG_TRACE << "Received RPC request {getNodeInfo}";
    auto& top_block = _core.getTopBlock();
    auto hash = base::Sha256::compute(base::toBytes(top_block));
    return { hash, top_block.getDepth() };
}


lk::Block RpcService::getBlock(const base::Sha256& block_hash)
{
    LOG_TRACE << "Received RPC request {getBlock} with block_hash[" << block_hash << "]";
    if (auto block_opt = _core.findBlock(block_hash); block_opt) {
        return *block_opt;
    }
    RAISE_ERROR(base::InvalidArgument, std::string("Block was not found. hash[hex]:") + block_hash.toHex());
}


lk::Block RpcService::getBlock(uint64_t block_number)
{
    LOG_TRACE << "Received RPC request {getBlock} with block_number[" << block_number << "]";
    if (auto block_hash_opt = _core.findBlockHash(block_number); block_hash_opt) {
        if (auto block_opt = _core.findBlock(*block_hash_opt); block_opt) {
            return *block_opt;
        }
    }
    RAISE_ERROR(base::InvalidArgument, std::string("Block was not found. number:") + std::to_string(block_number));
}


lk::Transaction RpcService::getTransaction(const base::Sha256& transaction_hash)
{
    LOG_TRACE << "Received RPC request {getTransaction}";
    if (auto transaction_opt = _core.findTransaction(transaction_hash); transaction_opt) {
        return *transaction_opt;
    }
    RAISE_ERROR(base::InvalidArgument, std::string("Transaction was not found. hash[hex]:") + transaction_hash.toHex());
}


lk::TransactionStatus RpcService::pushTransaction(const lk::Transaction& tx)
{
    LOG_TRACE << "Received RPC request {pushTransaction} with tx[" << tx << "]";
    return _core.addPendingTransaction(tx);
}


lk::TransactionStatus RpcService::getTransactionStatus(const base::Sha256& transaction_hash)
{
    LOG_TRACE << "Received RPC request {getTransactionStatus}";
    if (auto transaction_output_opt = _core.getTransactionOutput(transaction_hash); transaction_output_opt) {
        return *transaction_output_opt;
    }
    RAISE_ERROR(base::InvalidArgument,
                std::string("TransactionOutput was not found. hash[hex]:") + transaction_hash.toHex());
}


base::Bytes RpcService::callContractView(const lk::ViewCall& call)
{
    LOG_TRACE << "Received RPC request {callContractView}";
    return _core.callViewMethod(call);
}
