#include "http_adapter.hpp"
#include "tools.hpp"

#include <cpprest/asyncrt_utils.h>
#include <cpprest/json.h>
#include <cpprest/uri.h>

#include <string>


namespace rpc::http
{


class ActionBase
{
  public:
    //====================================
    explicit ActionBase(std::shared_ptr<rpc::BaseRpc>& service);
    virtual ~ActionBase() = default;
    //====================================
    virtual const std::string& getName() const = 0;
    virtual void run(web::json::value& result) = 0;
    //====================================
  protected:
    std::shared_ptr<rpc::BaseRpc>& _service;
};


ActionBase::ActionBase(std::shared_ptr<rpc::BaseRpc>& service)
  : _service{ service }
{}


class ActionNodeInfo : public ActionBase
{
  public:
    //====================================
    explicit ActionNodeInfo(std::shared_ptr<rpc::BaseRpc>& service);
    virtual ~ActionNodeInfo() = default;
    //====================================
    const std::string& getName() const override;
    void run(web::json::value& result) override;
};


ActionNodeInfo::ActionNodeInfo(std::shared_ptr<rpc::BaseRpc>& service)
  : ActionBase(service)
{}


const std::string& ActionNodeInfo::getName() const
{
    static const std::string name = "nodeInfo";
    return name;
}


void ActionNodeInfo::run(web::json::value& result)
{
    auto info = _service->getNodeInfo();
    result = serializeInfo(info);
}


class ActionJsonProcessBase : public ActionBase
{
  public:
    //====================================
    explicit ActionJsonProcessBase(web::json::value& input, std::shared_ptr<rpc::BaseRpc>& service);
    virtual ~ActionJsonProcessBase() = default;
    virtual bool loadArguments() = 0;
    //====================================
  protected:
    web::json::value _input;
};


ActionJsonProcessBase::ActionJsonProcessBase(web::json::value& input, std::shared_ptr<rpc::BaseRpc>& service)
  : ActionBase(service)
  , _input{ input }
{}


class ActionGetAccount : public ActionJsonProcessBase
{
  public:
    //====================================
    explicit ActionGetAccount(web::json::value& input, std::shared_ptr<rpc::BaseRpc>& service);
    virtual ~ActionGetAccount() = default;
    //====================================
    const std::string& getName() const override;
    bool loadArguments() override;
    void run(web::json::value& result) override;

  private:
    std::optional<lk::Address> _address;
};


ActionGetAccount::ActionGetAccount(web::json::value& input, std::shared_ptr<rpc::BaseRpc>& service)
  : ActionJsonProcessBase(input, service)
{}


const std::string& ActionGetAccount::getName() const
{
    static const std::string name = "get_account";
    return name;
}


bool ActionGetAccount::loadArguments()
{
    if (_input.has_field("address")) {
        _address = deserializeAddress(_input.at("address").as_string());
        return _address.has_value();
    }
    return false;
}


void ActionGetAccount::run(web::json::value& result)
{
    auto account_info = _service->getAccount(_address.value());
    result = serializeAccountInfo(account_info);
}


class ActionGetBlock : public ActionJsonProcessBase
{
  public:
    //====================================
    explicit ActionGetBlock(web::json::value& input, std::shared_ptr<rpc::BaseRpc>& service);
    virtual ~ActionGetBlock() = default;
    //====================================
    const std::string& getName() const override;
    bool loadArguments() override;
    void run(web::json::value& result) override;

  private:
    std::optional<base::Sha256> _block_hash;
    std::optional<lk::BlockDepth> _block_number;
};


ActionGetBlock::ActionGetBlock(web::json::value& input, std::shared_ptr<rpc::BaseRpc>& service)
  : ActionJsonProcessBase(input, service)
{}


const std::string& ActionGetBlock::getName() const
{
    static const std::string name = "get_block";
    return name;
}


bool ActionGetBlock::loadArguments()
{
    if (_input.has_field("hash")) {
        _block_hash = deserializeHash(_input.at("hash").as_string());
        return _block_hash.has_value();
    }
    else if (_input.has_field("number")) {
        _block_number = _input.at("number").as_number().to_uint64();
        return _block_number.has_value();
    }
    return false;
}


void ActionGetBlock::run(web::json::value& result)
{
    if (_block_hash) {
        auto block = _service->getBlock(_block_hash.value());
        result = serializeBlock(block);
    }
    else if (_block_number) {
        auto block = _service->getBlock(_block_number.value());
        result = serializeBlock(block);
    }
    else {
        RAISE_ERROR(base::InvalidArgument, "bad request");
    }
}


class ActionGetTransaction : public ActionJsonProcessBase
{
  public:
    //====================================
    explicit ActionGetTransaction(web::json::value& input, std::shared_ptr<rpc::BaseRpc>& service);
    virtual ~ActionGetTransaction() = default;
    //====================================
    const std::string& getName() const override;
    bool loadArguments() override;
    void run(web::json::value& result) override;

  private:
    std::optional<base::Sha256> _hash;
};


ActionGetTransaction::ActionGetTransaction(web::json::value& input, std::shared_ptr<rpc::BaseRpc>& service)
  : ActionJsonProcessBase(input, service)
{}


const std::string& ActionGetTransaction::getName() const
{
    static const std::string name = "get_transaction";
    return name;
}


bool ActionGetTransaction::loadArguments()
{
    if (_input.has_field("hash")) {
        _hash = deserializeHash(_input.at("hash").as_string());
        return _hash.has_value();
    }
    return false;
}


void ActionGetTransaction::run(web::json::value& result)
{
    auto tx = _service->getTransaction(_hash.value());
    result = serializeTransaction(tx);
}


class ActionGetTransactionResult : public ActionJsonProcessBase
{
  public:
    //====================================
    explicit ActionGetTransactionResult(web::json::value& input, std::shared_ptr<rpc::BaseRpc>& service);
    virtual ~ActionGetTransactionResult() = default;
    //====================================
    const std::string& getName() const override;
    bool loadArguments() override;
    void run(web::json::value& result) override;

  private:
    std::optional<base::Sha256> _hash;
};


ActionGetTransactionResult::ActionGetTransactionResult(web::json::value& input, std::shared_ptr<rpc::BaseRpc>& service)
  : ActionJsonProcessBase(input, service)
{}


const std::string& ActionGetTransactionResult::getName() const
{
    static const std::string name = "get_transaction_result";
    return name;
}


bool ActionGetTransactionResult::loadArguments()
{
    if (_input.has_field("hash")) {
        _hash = deserializeHash(_input.at("hash").as_string());
        return _hash.has_value();
    }
    return false;
}


void ActionGetTransactionResult::run(web::json::value& result)
{
    auto tx_result = _service->getTransactionResult(_hash.value());
    result = serializeTransactionStatus(tx_result);
}


class ActionPushTransaction : public ActionJsonProcessBase
{
  public:
    //====================================
    explicit ActionPushTransaction(web::json::value& input, std::shared_ptr<rpc::BaseRpc>& service);
    virtual ~ActionPushTransaction() = default;
    //====================================
    const std::string& getName() const override;
    bool loadArguments() override;
    void run(web::json::value& result) override;

  private:
    std::optional<lk::Transaction> _tx;
};


ActionPushTransaction::ActionPushTransaction(web::json::value& input, std::shared_ptr<rpc::BaseRpc>& service)
  : ActionJsonProcessBase(input, service)
{}


const std::string& ActionPushTransaction::getName() const
{
    static const std::string name = "push_transaction";
    return name;
}


bool ActionPushTransaction::loadArguments()
{
    _tx = deserializeTransaction(_input);
    return _tx.has_value();
}


void ActionPushTransaction::run(web::json::value& result)
{
    auto tx_result = _service->pushTransaction(_tx.value());
    result = serializeTransactionStatus(tx_result);
}


class ActionGetStorageValue : public ActionJsonProcessBase
{
  public:
    //====================================
    explicit ActionGetStorageValue(web::json::value& input, std::shared_ptr<rpc::BaseRpc>& service);
    virtual ~ActionGetStorageValue() = default;
    //====================================
    const std::string& getName() const override;
    bool loadArguments() override;
    void run(web::json::value& result) override;

  private:
    std::optional<lk::Address> _from_address;
    std::optional<lk::Address> _contract_address;
    std::optional<base::Bytes> _message;
};


ActionGetStorageValue::ActionGetStorageValue(web::json::value& input, std::shared_ptr<rpc::BaseRpc>& service)
  : ActionJsonProcessBase(input, service)
{}


const std::string& ActionGetStorageValue::getName() const
{
    static const std::string name = "get_storage_value";
    return name;
}


bool ActionGetStorageValue::loadArguments()
{
    if (_input.has_field("from")) {
        _from_address = deserializeAddress(_input.at("from").as_string());
    }
    if (_input.has_field("to")) {
        _contract_address = deserializeAddress(_input.at("to").as_string());
    }
    if (_input.has_field("message")) {
        _message = deserializeBytes(_input.at("message").as_string());
    }
    return _from_address.has_value() && _contract_address.has_value() && _message.has_value();
}


void ActionGetStorageValue::run(web::json::value& result)
{
    auto call_result = _service->callContractView(_from_address.value(), _contract_address.value(), _message.value());
    result = serializeBytes(call_result);
}


template<typename T>
web::json::value run_empty(std::shared_ptr<rpc::BaseRpc>& service)
{
    T action(service);
    web::json::value result;
    result["method"] = web::json::value::string(action.getName());
    try {
        web::json::value action_result;
        action.run(action_result);
        result["status"] = web::json::value::string("ok");
        result["result"] = action_result;
    }
    catch (const std::exception& e) {
        result["status"] = web::json::value::string("error");
    }
    return result;
}


template<typename T>
web::json::value run_json_process(const web::http::http_request& message, std::shared_ptr<rpc::BaseRpc>& service)
{
    web::json::value request_json;
    message.extract_json()
      .then([&request_json](web::json::value request_body) { request_json = std::move(request_body); })
      .wait();

    T action(request_json, service);
    web::json::value result;
    result["method"] = web::json::value::string(action.getName());

    try {
        if (!action.loadArguments()) {
            result["status"] = web::json::value::string("error");
            result["result"] = web::json::value::string("invalid input json");
        }
    }
    catch (const std::exception& e) {
        result["status"] = web::json::value::string("error");
        result["result"] = web::json::value::string("error at input deserialization");
    }

    try {
        web::json::value action_result;
        action.run(action_result);
        result["status"] = web::json::value::string("ok");
        result["result"] = action_result;
    }
    catch (const std::exception& e) {
        result["status"] = web::json::value::string("error");
    }
    return result;
}


void Adapter::init(std::shared_ptr<BaseRpc> service)
{
    _service = std::move(service);

    _empty_processors.insert({ "get_node_info", run_empty<ActionNodeInfo> });

    _json_processors.insert({ "get_account", run_json_process<ActionGetAccount> });
    _json_processors.insert({ "get_block", run_json_process<ActionGetBlock> });
    _json_processors.insert({ "get_transaction", run_json_process<ActionGetTransaction> });
    _json_processors.insert({ "get_transaction_result", run_json_process<ActionGetTransactionResult> });
    _json_processors.insert({ "push_transaction", run_json_process<ActionPushTransaction> });
    _json_processors.insert({ "get_storage_value", run_json_process<ActionGetStorageValue> });
}


void Adapter::handler(const web::http::http_request& message)
{
    LOG_ERROR << "registrated connection: " << message.remote_address();
    auto paths = web::http::uri::split_path(web::http::uri::decode(message.relative_uri().path()));

    if (paths.empty()) {
        LOG_ERROR << "no any route at request";
        web::json::value result;
        result["method"] = web::json::value::string("None");
        result["status"] = web::json::value::string("error");
        result["result"] = web::json::value::string("no any route at request");
        message.reply(web::http::status_codes::Forbidden, result);
    }

    auto root_path = paths[0];

    if (auto json_it = _json_processors.find(root_path); json_it != _json_processors.end()) {
        auto reply_json = json_it->second(message, _service);
        message.reply(web::http::status_codes::OK, reply_json);
        return;
    }
    else if (auto empty_it = _empty_processors.find(root_path); empty_it != _empty_processors.end()) {
        auto reply_json = empty_it->second(_service);
        message.reply(web::http::status_codes::OK, reply_json);
        return;
    }
    else {
        RAISE_ERROR(base::InvalidArgument, "processor was not found");
        LOG_ERROR << "no any processor wan found";
        web::json::value result;
        result["method"] = web::json::value::string("None");
        result["status"] = web::json::value::string("error");
        result["result"] = web::json::value::string("no any processor wan found");
        message.reply(web::http::status_codes::BadGateway, result);
    }
}

}