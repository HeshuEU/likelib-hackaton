#include "node.hpp"

#include <functional>

Node::Node(const base::PropertyTree& config)
  : _config{ config }
  , _key_vault(_config)
  , _core{ _config, _key_vault }
{
    auto service = std::make_shared<node::GeneralServerService>(_core);
    _rpc = rpc::create_rpc_server(_config, service);

    auto miner_callback = std::bind(&Node::onBlockMine, this, std::placeholders::_1);
    _miner = std::make_unique<Miner>(_config, miner_callback);

    _core.subscribeToNewPendingTransaction(std::bind(&Node::onNewTransactionReceived, this, std::placeholders::_1));
    _core.subscribeToBlockAddition(std::bind(&Node::onNewBlock, this, std::placeholders::_1));
}


void Node::run()
{
    _core.run(); // run before all others

    try {
        _rpc->run();
    }
    catch (const std::exception& e) {
        LOG_WARNING << "Cannot start RPC server: " << e.what();
    }
    catch (...) {
        LOG_WARNING << "Cannot start RPC server: unknown error";
    }
}


void Node::onBlockMine(lk::Block&& block)
{
    _core.tryAddBlock(block);
}


base::FixedBytes<impl::CommonData::COMPLEXITY_SIZE> Node::getMiningComplexity()
{
    base::FixedBytes<impl::CommonData::COMPLEXITY_SIZE> complexity;
    complexity[2] = 0xbf;
    return complexity;
}


void Node::onNewTransactionReceived(const lk::Transaction&)
{
    lk::Block block = _core.getBlockTemplate();
    if (!block.getTransactions().isEmpty()) {
        _miner->findNonce(_core.getBlockTemplate(), getMiningComplexity());
    }
    else {
        _miner->dropJob();
    }
}


void Node::onNewBlock(const lk::Block&)
{
    lk::Block block = _core.getBlockTemplate();
    if (!block.getTransactions().isEmpty()) {
        _miner->findNonce(_core.getBlockTemplate(), getMiningComplexity());
    }
    else {
        _miner->dropJob();
    }
}
