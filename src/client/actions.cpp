#include "actions.hpp"
#include "config.hpp"

#include <core/transaction.hpp>

#include <rpc/error.hpp>
#include <rpc/rpc.hpp>

#include <vm/tools.hpp>
#include <vm/vm.hpp>

#include <base/config.hpp>
#include <base/directory.hpp>
#include <base/hash.hpp>
#include <base/log.hpp>
#include <base/property_tree.hpp>
#include <base/subprogram_router.hpp>
#include <base/time.hpp>

#include <cstring>
#include <iostream>
#include <string>


//====================================

namespace
{

constexpr const char* HOST_OPTION = "host";
constexpr const char* IS_HTTP_CLIENT_OPTION = "http";
constexpr const char* TO_ADDRESS_OPTION = "to";
constexpr const char* AMOUNT_OPTION = "amount";
constexpr const char* KEYS_DIRECTORY_OPTION = "keys";
constexpr const char* FEE_OPTION = "fee";
constexpr const char* ADDRESS_OPTION = "address";
constexpr const char* CODE_PATH_OPTION = "code";
constexpr const char* METHOD_NAME_OPTION = "method";
constexpr const char* MESSAGE_OPTION = "message";
constexpr const char* HASH_OPTION = "hash";
constexpr const char* NUMBER_OPTION = "number";


std::pair<base::RsaPublicKey, base::RsaPrivateKey> loadKeys(const std::filesystem::path& dir)
{
    auto public_key_path = base::config::makePublicKeyPath(dir);
    if (!std::filesystem::exists(public_key_path)) {
        RAISE_ERROR(base::InaccessibleFile, "cannot find public key file by path \"" + public_key_path.string() + '\"');
    }
    auto public_key = base::RsaPublicKey::load(public_key_path);

    auto private_key_path = base::config::makePrivateKeyPath(dir);
    if (!std::filesystem::exists(private_key_path)) {
        RAISE_ERROR(base::InaccessibleFile,
                    "cannot find private key file by path \"" + private_key_path.string() + '\"');
    }
    auto private_key = base::RsaPrivateKey::load(private_key_path);

    return { std::move(public_key), std::move(private_key) };
}

bool checkOptionEmptyAndWriteMessage(const base::ProgramOptionsParser& parser, const char* const option)
{
    if (!parser.hasOption(option)) {
        std::cerr << "Option [" << option << "] was not found\n";
        LOG_ERROR << "Option [" << option << "] was not found";
        return true;
    }
    return false;
}

} // namespace

//====================================

ActionBase::ActionBase(base::SubprogramRouter& router)
  : _router{ router }
{}


int ActionBase::run()
{
    try {
        setupOptionsParser(_router.getOptionsParser());
        _router.update();

        if (_router.getOptionsParser().hasOption("help")) {
            std::cout << _router.helpMessage() << std::endl;
            return base::config::EXIT_OK;
        }

        if (auto ret = loadOptions(_router.getOptionsParser()); ret != base::config::EXIT_OK) {
            return ret;
        }
        if (auto ret = execute(); ret != base::config::EXIT_OK) {
            return ret;
        }
    }
    catch (const base::ParsingError& er) {
        std::cerr << "Invalid arguments";
        if (std::strlen(er.what())) {
            std::cerr << ": " << er.what();
        }
        std::cerr << "\n" << _router.helpMessage();
        LOG_ERROR << "[base::ParsingError caught during execution of Client::" << getName() << "] " << er.what();
        return base::config::EXIT_FAIL;
    }
    catch (const rpc::RpcError& er) {
        std::cerr << "RPC error " << er.what() << "\n";
        LOG_ERROR << "[rpc::RpcError caught during client::" << getName() << "] " << er.what();
        return base::config::EXIT_FAIL;
    }
    catch (const base::Error& er) {
        std::cerr << "Unexpected error." << er.what() << "\n";
        LOG_ERROR << "[base::Error caught during Client::" << getName() << "] " << er.what();
        return base::config::EXIT_FAIL;
    }
    catch (const std::exception& er) {
        std::cerr << "Unexpected error." << er.what() << "\n";
        LOG_ERROR << "[std::exception caught during Client::" << getName() << "] " << er.what();
        return base::config::EXIT_FAIL;
    }
    catch (...) {
        std::cerr << "Unexpected error.\n";
        LOG_ERROR << "[exception of an unknown type caught during client::" << getName() << "] ";
        return base::config::EXIT_FAIL;
    }

    return base::config::EXIT_OK;
}

//====================================

ActionTestConnection::ActionTestConnection(base::SubprogramRouter& router)
  : ActionBase{ router }
{}


const std::string_view& ActionTestConnection::getName() const
{
    static const std::string_view name = "TestConnection";
    return name;
}


void ActionTestConnection::setupOptionsParser(base::ProgramOptionsParser& parser)
{
    parser.addOption<std::string>(HOST_OPTION, "address of host");
    parser.addFlag(IS_HTTP_CLIENT_OPTION, "is set enable http client call");
}


int ActionTestConnection::loadOptions(const base::ProgramOptionsParser& parser)
{
    if (checkOptionEmptyAndWriteMessage(parser, HOST_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    _host_address = parser.getValue<std::string>(HOST_OPTION);
    _is_http_mode = parser.hasOption(IS_HTTP_CLIENT_OPTION);
    return base::config::EXIT_OK;
}


int ActionTestConnection::execute()
{
    LOG_INFO << "Test connect to rpc server by: " << _host_address;

    std::unique_ptr<rpc::BaseRpc> client;
    if (_is_http_mode) {
        client = rpc::createRpcClient(rpc::ClientMode::HTTP, _host_address);
    }
    else {
        client = rpc::createRpcClient(rpc::ClientMode::GRPC, _host_address);
    }

    auto answer = client->getNodeInfo();
    if (config::API_VERSION == answer.api_version) {
        std::cout << "Connection test passed" << std::endl;
        LOG_INFO << "Connection test passed";
    }
    else {
        std::cout << "Connection test failed" << std::endl;
        LOG_INFO << "Connection test failed\n";
    }
    return base::config::EXIT_OK;
}

//====================================

ActionNodeInfo::ActionNodeInfo(base::SubprogramRouter& router)
  : ActionBase{ router }
{}


const std::string_view& ActionNodeInfo::getName() const
{
    static const std::string_view name = "NodeInfo";
    return name;
}


void ActionNodeInfo::setupOptionsParser(base::ProgramOptionsParser& parser)
{
    parser.addOption<std::string>(HOST_OPTION, "address of host");
    parser.addFlag(IS_HTTP_CLIENT_OPTION, "is set enable http client call");
}


int ActionNodeInfo::loadOptions(const base::ProgramOptionsParser& parser)
{
    if (checkOptionEmptyAndWriteMessage(parser, HOST_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    _host_address = parser.getValue<std::string>(HOST_OPTION);
    _is_http_mode = parser.hasOption(IS_HTTP_CLIENT_OPTION);
    return base::config::EXIT_OK;
}


int ActionNodeInfo::execute()
{
    LOG_INFO << "Trying to connect to rpc server at " << _host_address;

    std::unique_ptr<rpc::BaseRpc> client;
    if (_is_http_mode) {
        client = rpc::createRpcClient(rpc::ClientMode::HTTP, _host_address);
    }
    else {
        client = rpc::createRpcClient(rpc::ClientMode::GRPC, _host_address);
    }

    auto info = client->getNodeInfo();

    std::cout << "Top block hash: " << info.top_block_hash << std::endl
              << "Top block number: " << info.top_block_number << std::endl;

    LOG_INFO << "Remote call of NodeInfo: top block hash[" << info.top_block_hash << "], top block number["
             << info.top_block_number << "]";
    return base::config::EXIT_OK;
}

//====================================

ActionGenerateKeys::ActionGenerateKeys(base::SubprogramRouter& router)
  : ActionBase{ router }
{}


const std::string_view& ActionGenerateKeys::getName() const
{
    static const std::string_view name = "GenerateKeys";
    return name;
}


void ActionGenerateKeys::setupOptionsParser(base::ProgramOptionsParser& parser)
{
    parser.addOption<std::string>(KEYS_DIRECTORY_OPTION, "directory in which a key pair will be generated");
}


int ActionGenerateKeys::loadOptions(const base::ProgramOptionsParser& parser)
{
    if (checkOptionEmptyAndWriteMessage(parser, KEYS_DIRECTORY_OPTION)) {
        return base::config::EXIT_FAIL;
    }

    _keys_dir = parser.getValue<std::string>(KEYS_DIRECTORY_OPTION);

    if (!std::filesystem::exists(_keys_dir)) {
        std::cerr << "Given path does not exist" << std::endl;
        return base::config::EXIT_FAIL;
    }
    else if (!std::filesystem::is_directory(_keys_dir)) {
        std::cerr << "Given path is not a directory" << std::endl;
        return base::config::EXIT_FAIL;
    }

    return base::config::EXIT_OK;
}


int ActionGenerateKeys::execute()
{
    LOG_INFO << "Generating key pair at " << _keys_dir;
    std::cout << "Generating key pair at " << _keys_dir << std::endl;

    const auto& [pub, priv] = base::generateKeys();

    auto public_path = base::config::makePublicKeyPath(_keys_dir);
    if (std::filesystem::exists(public_path)) {
        std::cerr << "Error: " << public_path << " already exists.\n";
        LOG_ERROR << public_path << " file already exists";
        return base::config::EXIT_FAIL;
    }

    auto private_path = base::config::makePrivateKeyPath(_keys_dir);
    if (std::filesystem::exists(private_path)) {
        std::cerr << "Error: " << private_path << " already exists.\n";
        LOG_ERROR << private_path << " file already exists";
        return base::config::EXIT_FAIL;
    }

    pub.save(public_path);

    std::cout << "Generated public key at " << public_path << std::endl;
    std::cout << "Address: " << lk::Address(pub) << std::endl;
    std::cout << "Hash of public key: " << base::Sha256::compute(pub.toBytes()) << std::endl;
    std::cout << "Hash of private key: " << base::Sha256::compute(priv.toBytes()) << std::endl;
    LOG_INFO << "Generated public key at " << public_path;

    priv.save(private_path);
    std::cout << "Generated private key at " << private_path << std::endl;
    LOG_INFO << "Generated private key at " << private_path;

    return base::config::EXIT_OK;
}

//====================================

ActionKeysInfo::ActionKeysInfo(base::SubprogramRouter& router)
  : ActionBase{ router }
{}


const std::string_view& ActionKeysInfo::getName() const
{
    static const std::string_view name = "KeysInfo";
    return name;
}


void ActionKeysInfo::setupOptionsParser(base::ProgramOptionsParser& parser)
{
    parser.addOption<std::string>(KEYS_DIRECTORY_OPTION, "directory with a key pair");
}


int ActionKeysInfo::loadOptions(const base::ProgramOptionsParser& parser)
{
    if (checkOptionEmptyAndWriteMessage(parser, KEYS_DIRECTORY_OPTION)) {
        return base::config::EXIT_FAIL;
    }

    _keys_dir = parser.getValue<std::string>(KEYS_DIRECTORY_OPTION);

    if (!std::filesystem::exists(_keys_dir)) {
        std::cerr << "Given path does not exist" << std::endl;
        return base::config::EXIT_FAIL;
    }
    else if (!std::filesystem::is_directory(_keys_dir)) {
        std::cerr << "Given path is not a directory" << std::endl;
        return base::config::EXIT_FAIL;
    }

    return base::config::EXIT_OK;
}


int ActionKeysInfo::execute()
{
    auto public_path = base::config::makePublicKeyPath(_keys_dir);
    if (!std::filesystem::exists(public_path)) {
        std::cerr << "Error: " << public_path << " doesn't exist.\n";
        LOG_ERROR << public_path << " file not exists";
        return base::config::EXIT_FAIL;
    }

    auto private_path = base::config::makePrivateKeyPath(_keys_dir);
    if (!std::filesystem::exists(private_path)) {
        std::cerr << "Error: " << private_path << " doesn't exist.\n";
        LOG_ERROR << private_path << " file not exists";
        return base::config::EXIT_FAIL;
    }

    auto pub = base::RsaPublicKey::load(public_path);
    auto priv = base::RsaPrivateKey::load(private_path);

    std::cout << "Address: " << lk::Address(pub) << std::endl;
    std::cout << "Hash of public key: " << base::Sha256::compute(pub.toBytes()) << std::endl;
    std::cout << "Hash of private key: " << base::Sha256::compute(priv.toBytes()) << std::endl;

    return base::config::EXIT_OK;
}

//====================================

ActionGetBalance::ActionGetBalance(base::SubprogramRouter& router)
  : ActionBase{ router }
{}


const std::string_view& ActionGetBalance::getName() const
{
    static const std::string_view name = "GetBalance";
    return name;
}


void ActionGetBalance::setupOptionsParser(base::ProgramOptionsParser& parser)
{
    parser.addOption<std::string>(HOST_OPTION, "address of host");
    parser.addOption<std::string>(ADDRESS_OPTION, "address of target account");
    parser.addFlag(IS_HTTP_CLIENT_OPTION, "is set enable http client call");
}


int ActionGetBalance::loadOptions(const base::ProgramOptionsParser& parser)
{
    if (checkOptionEmptyAndWriteMessage(parser, HOST_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    _host_address = parser.getValue<std::string>(HOST_OPTION);

    if (checkOptionEmptyAndWriteMessage(parser, ADDRESS_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    _account_address = lk::Address{ parser.getValue<std::string>(ADDRESS_OPTION) };

    _is_http_mode = parser.hasOption(IS_HTTP_CLIENT_OPTION);

    return base::config::EXIT_OK;
}


int ActionGetBalance::execute()
{
    LOG_INFO << "GetBalance for address " << _account_address << " from rpc server at " << _host_address;

    std::unique_ptr<rpc::BaseRpc> client;
    if (_is_http_mode) {
        client = rpc::createRpcClient(rpc::ClientMode::HTTP, _host_address);
    }
    else {
        client = rpc::createRpcClient(rpc::ClientMode::GRPC, _host_address);
    }

    auto result = client->getAccount(_account_address);
    std::cout << "Balance of " << _account_address << " is " << result.balance << std::endl;

    LOG_INFO << "Remote call of GetBalance(" << _account_address << ") -> [" << result.balance << "]";
    return base::config::EXIT_OK;
}

//====================================

ActionGetAccountInfo::ActionGetAccountInfo(base::SubprogramRouter& router)
  : ActionBase{ router }
{}


const std::string_view& ActionGetAccountInfo::getName() const
{
    static const std::string_view name = "GetAccountInfo";
    return name;
}


void ActionGetAccountInfo::setupOptionsParser(base::ProgramOptionsParser& parser)
{
    parser.addOption<std::string>(HOST_OPTION, "address of host");
    parser.addOption<std::string>(ADDRESS_OPTION, "address of target account");
    parser.addFlag(IS_HTTP_CLIENT_OPTION, "is set enable http client call");
}


int ActionGetAccountInfo::loadOptions(const base::ProgramOptionsParser& parser)
{
    if (checkOptionEmptyAndWriteMessage(parser, HOST_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    _host_address = parser.getValue<std::string>(HOST_OPTION);

    if (checkOptionEmptyAndWriteMessage(parser, ADDRESS_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    _account_address = lk::Address{ parser.getValue<std::string>(ADDRESS_OPTION) };

    _is_http_mode = parser.hasOption(IS_HTTP_CLIENT_OPTION);

    return base::config::EXIT_OK;
}


int ActionGetAccountInfo::execute()
{
    LOG_INFO << "GetAccountInfo for address " << _account_address << " from rpc server at " << _host_address;

    std::unique_ptr<rpc::BaseRpc> client;
    if (_is_http_mode) {
        client = rpc::createRpcClient(rpc::ClientMode::HTTP, _host_address);
    }
    else {
        client = rpc::createRpcClient(rpc::ClientMode::GRPC, _host_address);
    }

    auto result = client->getAccount(_account_address);

    if (result.type == lk::AccountType::CLIENT) {
        std::cout << "Client address: " << result.address.toString() << '\n'
                  << "\tBalance: " << result.balance << '\n'
                  << "\tNonce: " << result.nonce << '\n'
                  << "\tTransactions hashes: [" << std::endl;

        for (const auto& tx_hs : result.transactions_hashes) {
            std::cout << "\t\t" << tx_hs << std::endl;
        }
        std::cout << "\t\t]" << std::endl;
        LOG_INFO << "Remote call of GetAccountInfo(" << _account_address << ") -> balance[" << result.balance
                 << "], nonce[" << result.nonce << "], transactions[" << result.transactions_hashes.size() << "]";
    }
    else {
        std::cout << "Contract address: " << result.address.toString() << '\n'
                  << "\tBalance: " << result.balance << '\n'
                  << "\tABI: \n"
                  << result.serialized_abi << std::endl;

        LOG_INFO << "Remote call of GetAccountInfo(" << _account_address << ") -> balance[" << result.balance
                 << "], ABI[" << result.serialized_abi << "]";
    }

    return base::config::EXIT_OK;
}

//====================================

ActionCompile::ActionCompile(base::SubprogramRouter& router)
  : ActionBase{ router }
{}


const std::string_view& ActionCompile::getName() const
{
    static const std::string_view name = "Compile";
    return name;
}


void ActionCompile::setupOptionsParser(base::ProgramOptionsParser& parser)
{
    parser.addOption<std::string>(CODE_PATH_OPTION, "path to a Solidity code");
}


int ActionCompile::loadOptions(const base::ProgramOptionsParser& parser)
{
    if (checkOptionEmptyAndWriteMessage(parser, CODE_PATH_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    _code_file_path = parser.getValue<std::string>(CODE_PATH_OPTION);

    return base::config::EXIT_OK;
}


int ActionCompile::execute()
{

    std::optional<vm::Contracts> contracts;
    try {
        contracts = vm::compile(_code_file_path);
    }
    catch (const base::ParsingError& er) {
        std::cerr << er;
        return base::config::EXIT_FAIL;
    }
    catch (const base::SystemCallFailed& er) {
        std::cerr << er;
        return base::config::EXIT_FAIL;
    }

    if (!contracts) {
        std::cerr << "Compilation error\n";
        return base::config::EXIT_FAIL;
    }

    std::cout << "Compiled contracts:" << std::endl;
    for (const auto& contract : contracts.value()) {
        std::cout << "\t" << contract.name << std::endl;
        try {
            std::filesystem::path current_folder{ contract.name };
            base::createIfNotExists(current_folder);

            {
                std::ofstream file;
                file.open(current_folder / std::filesystem::path{ config::CONTRACT_BINARY_FILE });
                file << base::toHex(contract.code);
                file.close();
            }

            base::save(contract.metadata, current_folder / std::filesystem::path{ config::METADATA_JSON_FILE });
        }
        catch (const base::Error& er) {
            std::cerr << er;
            LOG_ERROR << er;
            return base::config::EXIT_FAIL;
        }
        catch (...) {
            std::cerr << "unexpected error at saving contract:" << contract.name;
            LOG_ERROR << "unexpected error at saving contract:" << contract.name;
            return base::config::EXIT_FAIL;
        }
    }

    return base::config::EXIT_OK;
}

//====================================

ActionEncode::ActionEncode(base::SubprogramRouter& router)
  : ActionBase{ router }
{}


const std::string_view& ActionEncode::getName() const
{
    static const std::string_view name = "Encode";
    return name;
}


void ActionEncode::setupOptionsParser(base::ProgramOptionsParser& parser)
{
    parser.addOption<std::string>(CODE_PATH_OPTION, "path to folder with compiled Solidity code");
    parser.addOption<std::string>(MESSAGE_OPTION, "call code");
}


int ActionEncode::loadOptions(const base::ProgramOptionsParser& parser)
{
    if (checkOptionEmptyAndWriteMessage(parser, CODE_PATH_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    _compiled_code_folder_path = parser.getValue<std::string>(CODE_PATH_OPTION);

    if (checkOptionEmptyAndWriteMessage(parser, MESSAGE_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    _call_data = parser.getValue<std::string>(MESSAGE_OPTION);

    return base::config::EXIT_OK;
}


int ActionEncode::execute()
{
    try {
        auto output_message = vm::encodeCall(_compiled_code_folder_path, _call_data);
        if (output_message) {
            std::cout << output_message.value() << std::endl;
        }
        else {
            std::cerr << "encoding failed.\n";
            return base::config::EXIT_FAIL;
        }
    }
    catch (const base::ParsingError& er) {
        std::cerr << er;
        return base::config::EXIT_FAIL;
    }
    catch (const base::SystemCallFailed& er) {
        std::cerr << er;
        return base::config::EXIT_FAIL;
    }

    return base::config::EXIT_OK;
}

//====================================

ActionDecode::ActionDecode(base::SubprogramRouter& router)
  : ActionBase{ router }
{}


const std::string_view& ActionDecode::getName() const
{
    static const std::string_view name = "Decode";
    return name;
}


void ActionDecode::setupOptionsParser(base::ProgramOptionsParser& parser)
{
    parser.addOption<std::string>(CODE_PATH_OPTION, "path to folder with compiled Solidity code");
    parser.addOption<std::string>(METHOD_NAME_OPTION, "call code");
    parser.addOption<std::string>(MESSAGE_OPTION, "data to decode");
}


int ActionDecode::loadOptions(const base::ProgramOptionsParser& parser)
{
    if (checkOptionEmptyAndWriteMessage(parser, CODE_PATH_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    _compiled_code_folder_path = parser.getValue<std::string>(CODE_PATH_OPTION);

    if (checkOptionEmptyAndWriteMessage(parser, METHOD_NAME_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    _method_name = parser.getValue<std::string>(METHOD_NAME_OPTION);

    if (checkOptionEmptyAndWriteMessage(parser, MESSAGE_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    _data_to_decode = parser.getValue<std::string>(MESSAGE_OPTION);
    return base::config::EXIT_OK;
}


int ActionDecode::execute()
{
    try {
        auto output_message = vm::decodeOutput(_compiled_code_folder_path, _method_name, _data_to_decode);
        if (output_message) {
            std::cout << output_message.value() << std::endl;
        }
        else {
            std::cerr << "decoding failed.\n";
            return base::config::EXIT_FAIL;
        }
    }
    catch (const base::ParsingError& er) {
        std::cerr << er;
        return base::config::EXIT_FAIL;
    }
    catch (const base::SystemCallFailed& er) {
        std::cerr << er;
        return base::config::EXIT_FAIL;
    }

    return base::config::EXIT_OK;
}

//====================================

ActionTransfer::ActionTransfer(base::SubprogramRouter& router)
  : ActionBase{ router }
{}


const std::string_view& ActionTransfer::getName() const
{
    static const std::string_view name = "Transfer";
    return name;
}


void ActionTransfer::setupOptionsParser(base::ProgramOptionsParser& parser)
{
    parser.addOption<std::string>(HOST_OPTION, "address of host");
    parser.addOption<std::string>(TO_ADDRESS_OPTION, "address of recipient account");
    parser.addOption<lk::Balance>(AMOUNT_OPTION, "amount count");
    parser.addOption<std::string>(KEYS_DIRECTORY_OPTION, "path to a directory with keys");
    parser.addOption<std::uint64_t>(FEE_OPTION, "fee count");
    parser.addFlag(IS_HTTP_CLIENT_OPTION, "is set enable http client call");
}


int ActionTransfer::loadOptions(const base::ProgramOptionsParser& parser)
{
    if (checkOptionEmptyAndWriteMessage(parser, HOST_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    _host_address = parser.getValue<std::string>(HOST_OPTION);

    if (checkOptionEmptyAndWriteMessage(parser, TO_ADDRESS_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    _to_address = lk::Address{ parser.getValue<std::string>(TO_ADDRESS_OPTION) };

    if (checkOptionEmptyAndWriteMessage(parser, AMOUNT_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    _amount = parser.getValue<lk::Balance>(AMOUNT_OPTION);

    if (checkOptionEmptyAndWriteMessage(parser, FEE_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    _fee = parser.getValue<std::uint64_t>(FEE_OPTION);

    if (checkOptionEmptyAndWriteMessage(parser, KEYS_DIRECTORY_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    _keys_dir = parser.getValue<std::string>(KEYS_DIRECTORY_OPTION);

    _is_http_mode = parser.hasOption(IS_HTTP_CLIENT_OPTION);

    return base::config::EXIT_OK;
}


int ActionTransfer::execute()
{
    auto [pub, priv] = loadKeys(_keys_dir);
    auto from_address = lk::Address(pub);

    lk::TransactionBuilder txb;
    txb.setFrom(std::move(from_address));
    txb.setTo(std::move(_to_address));
    txb.setAmount(_amount);
    txb.setTimestamp(base::Time::now());
    txb.setFee(_fee);
    txb.setData({});
    auto tx = std::move(txb).build();

    tx.sign(pub, priv);

    auto tx_hash = tx.hashOfTransaction();
    std::cout << "Created transaction with hash[hex]: " << tx_hash << std::endl;

    LOG_INFO << "Transfer from " << from_address << " to " << _to_address << " with amount " << _amount
             << " to rpc server " << _host_address;
    std::unique_ptr<rpc::BaseRpc> client;
    if (_is_http_mode) {
        client = rpc::createRpcClient(rpc::ClientMode::HTTP, _host_address);
    }
    else {
        client = rpc::createRpcClient(rpc::ClientMode::GRPC, _host_address);
    }
    auto status = client->pushTransaction(tx);

    if (status) {
        std::cout << "Transaction successfully performed" << std::endl;
    }
    else {
        std::cerr << "Transaction failed with message: " << status.getMessage() << std::endl;
    }

    LOG_INFO << "Remote call of Transfer(" << tx_hash << ") -> [" << status.getMessage() << "]";

    return base::config::EXIT_OK;
}

//====================================

ActionPushContract::ActionPushContract(base::SubprogramRouter& router)
  : ActionBase{ router }
{}


const std::string_view& ActionPushContract::getName() const
{
    static const std::string_view name = "PushContract";
    return name;
}


void ActionPushContract::setupOptionsParser(base::ProgramOptionsParser& parser)
{
    parser.addOption<std::string>(HOST_OPTION, "address of host");
    parser.addOption<lk::Balance>(AMOUNT_OPTION, "amount of Lk to transfer");
    parser.addOption<std::uint64_t>(FEE_OPTION, "fee count");
    parser.addOption<std::string>(KEYS_DIRECTORY_OPTION, "path to a directory with keys");
    parser.addOption<std::string>(MESSAGE_OPTION, "message for initialize smart contract");
    parser.addOption<std::string>(CODE_PATH_OPTION, "compiled contract code folder");
    parser.addFlag(IS_HTTP_CLIENT_OPTION, "is set enable http client call");
}


int ActionPushContract::loadOptions(const base::ProgramOptionsParser& parser)
{
    if (checkOptionEmptyAndWriteMessage(parser, HOST_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    _host_address = parser.getValue<std::string>(HOST_OPTION);

    if (checkOptionEmptyAndWriteMessage(parser, KEYS_DIRECTORY_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    _keys_dir = parser.getValue<std::string>(KEYS_DIRECTORY_OPTION);

    if (checkOptionEmptyAndWriteMessage(parser, AMOUNT_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    _amount = parser.getValue<lk::Balance>(AMOUNT_OPTION);

    if (checkOptionEmptyAndWriteMessage(parser, FEE_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    _fee = parser.getValue<std::uint64_t>(FEE_OPTION);

    if (parser.hasOption(MESSAGE_OPTION)) {
        _message = base::fromHex<base::Bytes>(parser.getValue<std::string>(MESSAGE_OPTION));
    }

    if (checkOptionEmptyAndWriteMessage(parser, CODE_PATH_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    std::filesystem::path code_folder_path = parser.getValue<std::string>(CODE_PATH_OPTION);
    auto code_abi_file_path = code_folder_path / std::filesystem::path(config::METADATA_JSON_FILE);
    if (!std::filesystem::exists(code_abi_file_path)) {
        RAISE_ERROR(base::InvalidArgument,
                    "the file with this path[" + code_abi_file_path.string() + "] does not exist");
    }
    auto metadata = base::readConfig(code_abi_file_path);
    static constexpr const char* ABI_PATH = "output";
    _contract_abi = metadata.getSubTree(ABI_PATH);

    if (_message.isEmpty()) {
        auto code_binary_file_path = code_folder_path / std::filesystem::path(config::CONTRACT_BINARY_FILE);
        if (!std::filesystem::exists(code_binary_file_path)) {
            RAISE_ERROR(base::InvalidArgument,
                        "the file with this path[" + code_binary_file_path.string() + "] does not exist");
        }
        std::ifstream file(code_binary_file_path, std::ios::binary);
        auto buf = std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        _message = base::fromHex<base::Bytes>(buf);
    }

    _is_http_mode = parser.hasOption(IS_HTTP_CLIENT_OPTION);

    return base::config::EXIT_OK;
}


int ActionPushContract::execute()
{
    auto [pub, priv] = loadKeys(_keys_dir);
    auto from_address = lk::Address(pub);

    lk::TransactionBuilder txb;
    txb.setAmount(_amount);
    txb.setFrom(std::move(from_address));
    txb.setTo(lk::Address::null());
    txb.setTimestamp(base::Time::now());
    txb.setFee(_fee);

    lk::ContractData data(_message, _contract_abi);
    txb.setData(base::toBytes(data));

    auto tx = std::move(txb).build();
    tx.sign(pub, priv);

    auto tx_hash = tx.hashOfTransaction();
    std::cout << "Created transaction with hash[hex]: " << tx_hash << std::endl;

    LOG_INFO << "Trying to connect to rpc server by: " << _host_address;
    std::unique_ptr<rpc::BaseRpc> client;
    if (_is_http_mode) {
        client = rpc::createRpcClient(rpc::ClientMode::HTTP, _host_address);
    }
    else {
        client = rpc::createRpcClient(rpc::ClientMode::GRPC, _host_address);
    }

    auto status = client->pushTransaction(tx);

    if (status) {
        lk::Address contract_address(base::base58Decode(status.getMessage()));
        std::cout << "Remote call of creation smart contract success -> contract created at ["
                  << contract_address.toString() << "], fee left[" << status.getFeeLeft() << "]" << std::endl;
        return base::config::EXIT_OK;
    }
    else {
        std::cout << "Remote call of creation smart contract is failed -> [" << status.getMessage() << "]" << std::endl;
        return base::config::EXIT_FAIL;
    }

    LOG_INFO << "Remote call of PushContract(" << tx_hash << ") -> [" << status.getMessage() << "]";
    return base::config::EXIT_OK;
}

//====================================

ActionContractCall::ActionContractCall(base::SubprogramRouter& router)
  : ActionBase{ router }
{}


const std::string_view& ActionContractCall::getName() const
{
    static const std::string_view name = "ContractCall";
    return name;
}


void ActionContractCall::setupOptionsParser(base::ProgramOptionsParser& parser)
{
    parser.addOption<std::string>(HOST_OPTION, "address of host");
    parser.addOption<std::string>(TO_ADDRESS_OPTION, "address of \"to\" contract");
    parser.addOption<lk::Balance>(AMOUNT_OPTION, "amount count");
    parser.addOption<std::uint64_t>(FEE_OPTION, "fee count");
    parser.addOption<std::string>(KEYS_DIRECTORY_OPTION, "path to a directory with keys");
    parser.addOption<std::string>(MESSAGE_OPTION, "message for call smart contract");
    parser.addFlag(IS_HTTP_CLIENT_OPTION, "is set enable http client call");
}


int ActionContractCall::loadOptions(const base::ProgramOptionsParser& parser)
{
    if (checkOptionEmptyAndWriteMessage(parser, HOST_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    _host_address = parser.getValue<std::string>(HOST_OPTION);

    if (checkOptionEmptyAndWriteMessage(parser, TO_ADDRESS_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    _to_address = lk::Address{ parser.getValue<std::string>(TO_ADDRESS_OPTION) };

    if (checkOptionEmptyAndWriteMessage(parser, AMOUNT_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    _amount = parser.getValue<lk::Balance>(AMOUNT_OPTION);

    if (checkOptionEmptyAndWriteMessage(parser, FEE_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    _fee = parser.getValue<std::uint64_t>(FEE_OPTION);

    if (checkOptionEmptyAndWriteMessage(parser, KEYS_DIRECTORY_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    _keys_dir = parser.getValue<std::string>(KEYS_DIRECTORY_OPTION);

    if (checkOptionEmptyAndWriteMessage(parser, MESSAGE_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    _message = parser.getValue<std::string>(MESSAGE_OPTION);

    _is_http_mode = parser.hasOption(IS_HTTP_CLIENT_OPTION);

    return base::config::EXIT_OK;
}


int ActionContractCall::execute()
{
    auto [pub, priv] = loadKeys(_keys_dir);
    auto from_address = lk::Address(pub);

    lk::TransactionBuilder txb;
    txb.setAmount(_amount);
    txb.setFrom(std::move(from_address));
    txb.setTo(std::move(_to_address));
    txb.setTimestamp(base::Time::now());
    txb.setFee(_fee);
    txb.setData(base::fromHex<base::Bytes>(_message));

    auto tx = std::move(txb).build();
    tx.sign(pub, priv);

    auto tx_hash = tx.hashOfTransaction();
    std::cout << "Created transaction with hash[hex]: " << tx_hash << std::endl;

    LOG_INFO << "Try to connect to rpc server by: " << _host_address;
    std::unique_ptr<rpc::BaseRpc> client;
    if (_is_http_mode) {
        client = rpc::createRpcClient(rpc::ClientMode::HTTP, _host_address);
    }
    else {
        client = rpc::createRpcClient(rpc::ClientMode::GRPC, _host_address);
    }

    auto status = client->pushTransaction(tx);

    if (status) {
        auto success_message = base::base64Decode(status.getMessage());
        std::cout << "Remote call of smart contract call success -> contract response[" << base::toHex(success_message)
                  << "], fee left[" << status.getFeeLeft() << "]" << std::endl;
        return base::config::EXIT_OK;
    }
    else {
        std::cout << "Remote call of smart contract call is failed -> [" << status.getMessage() << "]" << std::endl;
        return base::config::EXIT_FAIL;
    }

    return base::config::EXIT_OK;
}

//====================================

ActionCallContractView::ActionCallContractView(base::SubprogramRouter& router)
  : ActionBase{ router }
{}


const std::string_view& ActionCallContractView::getName() const
{
    static const std::string_view name = "CallContractView";
    return name;
}


void ActionCallContractView::setupOptionsParser(base::ProgramOptionsParser& parser)
{
    parser.addOption<std::string>(HOST_OPTION, "address of host");
    parser.addOption<std::string>(TO_ADDRESS_OPTION, "address of \"to\" contract");
    parser.addOption<std::string>(KEYS_DIRECTORY_OPTION, "path to a directory with keys");
    parser.addOption<std::string>(MESSAGE_OPTION, "message for call smart contract");
    parser.addFlag(IS_HTTP_CLIENT_OPTION, "is set enable http client call");
}


int ActionCallContractView::loadOptions(const base::ProgramOptionsParser& parser)
{
    if (checkOptionEmptyAndWriteMessage(parser, HOST_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    _host_address = parser.getValue<std::string>(HOST_OPTION);

    if (checkOptionEmptyAndWriteMessage(parser, TO_ADDRESS_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    _to_address = lk::Address{ parser.getValue<std::string>(TO_ADDRESS_OPTION) };

    if (checkOptionEmptyAndWriteMessage(parser, KEYS_DIRECTORY_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    _keys_dir = parser.getValue<std::string>(KEYS_DIRECTORY_OPTION);

    if (checkOptionEmptyAndWriteMessage(parser, MESSAGE_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    _message = parser.getValue<std::string>(MESSAGE_OPTION);

    _is_http_mode = parser.hasOption(IS_HTTP_CLIENT_OPTION);

    return base::config::EXIT_OK;
}


int ActionCallContractView::execute()
{
    auto [pub, priv] = loadKeys(_keys_dir);
    auto from_address = lk::Address(pub);

    LOG_INFO << "Try to connect to rpc server by: " << _host_address;
    std::unique_ptr<rpc::BaseRpc> client;
    if (_is_http_mode) {
        client = rpc::createRpcClient(rpc::ClientMode::HTTP, _host_address);
    }
    else {
        client = rpc::createRpcClient(rpc::ClientMode::GRPC, _host_address);
    }

    auto data = base::fromHex<base::Bytes>(_message);
    auto responce = client->callContractView(from_address, _to_address, std::move(data));

    std::cout << "View of smart contract responce: " << base::toHex(responce) << std::endl;

    return base::config::EXIT_OK;
}

//====================================

ActionGetTransaction::ActionGetTransaction(base::SubprogramRouter& router)
  : ActionBase{ router }
{}


const std::string_view& ActionGetTransaction::getName() const
{
    static const std::string_view name = "GetTransaction";
    return name;
}


void ActionGetTransaction::setupOptionsParser(base::ProgramOptionsParser& parser)
{
    parser.addOption<std::string>(HOST_OPTION, "address of host");
    parser.addOption<std::string>(HASH_OPTION, "transaction hash hex");
    parser.addFlag(IS_HTTP_CLIENT_OPTION, "is set enable http client call");
}


int ActionGetTransaction::loadOptions(const base::ProgramOptionsParser& parser)
{
    if (checkOptionEmptyAndWriteMessage(parser, HOST_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    _host_address = parser.getValue<std::string>(HOST_OPTION);

    if (checkOptionEmptyAndWriteMessage(parser, HASH_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    _transaction_hash = base::Sha256{ base::fromHex<base::Bytes>(parser.getValue<std::string>(HASH_OPTION)) };

    _is_http_mode = parser.hasOption(IS_HTTP_CLIENT_OPTION);

    return base::config::EXIT_OK;
}


int ActionGetTransaction::execute()
{
    LOG_INFO << "Try to connect to rpc server by: " << _host_address;
    std::unique_ptr<rpc::BaseRpc> client;
    if (_is_http_mode) {
        client = rpc::createRpcClient(rpc::ClientMode::HTTP, _host_address);
    }
    else {
        client = rpc::createRpcClient(rpc::ClientMode::GRPC, _host_address);
    }
    auto tx = client->getTransaction(_transaction_hash);

    if (tx.getTimestamp().getSecondsSinceEpoch() == 0 && tx.getFrom().isNull()) {
        std::cout << "Cannot find given transaction" << std::endl;
        return base::config::EXIT_OK;
    }

    std::string type_message;
    if (tx.getTo() == lk::Address::null()) {
        type_message = "contract creation";
    }
    else if (tx.getData().isEmpty()) {
        type_message = "transfer";
    }
    else {
        type_message = "contract call";
    }

    std::cout << "\tType: " << type_message << '\n'
              << "\tFrom: " << tx.getFrom().toString() << '\n'
              << "\tTo: " << tx.getTo().toString() << '\n'
              << "\tValue: " << tx.getAmount() << '\n'
              << "\tFee: " << tx.getFee() << '\n'
              << "\tTimestamp: " << tx.getTimestamp() << '\n'
              << "\tData: " << (tx.getData().isEmpty() ? "<empty>" : base::toHex(tx.getData())) << '\n'
              << "\tSignature: " << (tx.checkSign() ? "verified" : "bad signature") << std::endl;

    std::cout.flush();

    return base::config::EXIT_OK;
}

//====================================

ActionGetTransactionStatus::ActionGetTransactionStatus(base::SubprogramRouter& router)
  : ActionBase{ router }
{}


const std::string_view& ActionGetTransactionStatus::getName() const
{
    static const std::string_view name = "GetTransactionStatus";
    return name;
}


void ActionGetTransactionStatus::setupOptionsParser(base::ProgramOptionsParser& parser)
{
    parser.addOption<std::string>(HOST_OPTION, "address of host");
    parser.addOption<std::string>(HASH_OPTION, "transaction hash hex");
    parser.addFlag(IS_HTTP_CLIENT_OPTION, "is set enable http client call");
}


int ActionGetTransactionStatus::loadOptions(const base::ProgramOptionsParser& parser)
{
    if (checkOptionEmptyAndWriteMessage(parser, HOST_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    _host_address = parser.getValue<std::string>(HOST_OPTION);

    if (checkOptionEmptyAndWriteMessage(parser, HASH_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    _transaction_hash = base::Sha256{ base::fromHex<base::Bytes>(parser.getValue<std::string>(HASH_OPTION)) };

    _is_http_mode = parser.hasOption(IS_HTTP_CLIENT_OPTION);

    return base::config::EXIT_OK;
}


int ActionGetTransactionStatus::execute()
{
    LOG_INFO << "Try to connect to rpc server by: " << _host_address;
    std::unique_ptr<rpc::BaseRpc> client;
    if (_is_http_mode) {
        client = rpc::createRpcClient(rpc::ClientMode::HTTP, _host_address);
    }
    else {
        client = rpc::createRpcClient(rpc::ClientMode::GRPC, _host_address);
    }
    auto result = client->getTransactionResult(_transaction_hash);

    std::string type_message;

    switch (result.getType()) {
        case lk::TransactionStatus::ActionType::Transfer:
            type_message = "transfer";
            break;
        case lk::TransactionStatus::ActionType::ContractCall:
            type_message = "contract call";
            break;
        case lk::TransactionStatus::ActionType::ContractCreation:
            type_message = "contract creation";
            break;
        case lk::TransactionStatus::ActionType::None:
            std::cout << "Cannot find given transaction result" << std::endl;
            return base::config::EXIT_OK;
    }

    std::string status_message;
    switch (result.getStatus()) {
        case lk::TransactionStatus::StatusCode::Success:
            status_message = "success";
            break;
        case lk::TransactionStatus::StatusCode::Rejected:
            status_message = "rejected";
            break;
        case lk::TransactionStatus::StatusCode::Revert:
            status_message = "revert";
            break;
        case lk::TransactionStatus::StatusCode::Failed:
            status_message = "failed";
            break;
    }

    std::cout << "\tType: " << type_message << '\n'
              << "\tStatus: " << status_message << '\n'
              << "\tFee left: " << result.getFeeLeft() << '\n';

    if (result.getStatus() == lk::TransactionStatus::StatusCode::Success &&
        result.getType() == lk::TransactionStatus::ActionType::ContractCreation) {
        std::cout << "\tMessage: "
                  << "new contract address " << lk::Address{ result.getMessage() } << std::endl;
    }
    else if (result.getStatus() == lk::TransactionStatus::StatusCode::Success &&
             result.getType() == lk::TransactionStatus::ActionType::ContractCall) {
        std::cout << "\tMessage: "
                  << "new contract address " << base::toHex(base::base64Decode(result.getMessage())) << std::endl;
    }
    else {
        std::cout << "\tMessage: " << result.getMessage() << std::endl;
    }

    std::cout.flush();

    return base::config::EXIT_OK;
}

//====================================

ActionGetBlock::ActionGetBlock(base::SubprogramRouter& router)
  : ActionBase{ router }
{}


const std::string_view& ActionGetBlock::getName() const
{
    static const std::string_view name = "GetBlock";
    return name;
}


void ActionGetBlock::setupOptionsParser(base::ProgramOptionsParser& parser)
{
    parser.addOption<std::string>(HOST_OPTION, "address of host");
    parser.addOption<std::string>(HASH_OPTION, "block hash hex");
    parser.addOption<std::uint64_t>(NUMBER_OPTION, "block number");
    parser.addFlag(IS_HTTP_CLIENT_OPTION, "is set enable http client call");
}


int ActionGetBlock::loadOptions(const base::ProgramOptionsParser& parser)
{
    if (checkOptionEmptyAndWriteMessage(parser, HOST_OPTION)) {
        return base::config::EXIT_FAIL;
    }
    _host_address = parser.getValue<std::string>(HOST_OPTION);

    if (parser.hasOption(HASH_OPTION)) {
        _block_hash = base::Sha256{ base::fromHex<base::Bytes>(parser.getValue<std::string>(HASH_OPTION)) };
    }
    else if (parser.hasOption(NUMBER_OPTION)) {
        _block_number = parser.getValue<std::uint64_t>(NUMBER_OPTION);
    }
    else {
        return base::config::EXIT_FAIL;
    }

    _is_http_mode = parser.hasOption(IS_HTTP_CLIENT_OPTION);

    return base::config::EXIT_OK;
}


int ActionGetBlock::execute()
{
    LOG_INFO << "Try to connect to rpc server by: " << _host_address;
    std::unique_ptr<rpc::BaseRpc> client;
    if (_is_http_mode) {
        client = rpc::createRpcClient(rpc::ClientMode::HTTP, _host_address);
    }
    else {
        client = rpc::createRpcClient(rpc::ClientMode::GRPC, _host_address);
    }

    std::optional<lk::Block> block_option;
    if (_block_hash == base::Sha256::null()) {
        block_option = client->getBlock(_block_number);
    }
    else {
        block_option = client->getBlock(_block_hash);
    }

    auto block = block_option.value();
    if (block.getTimestamp().getSecondsSinceEpoch() == 0 && block.getDepth() == lk::BlockDepth(-1)) {
        std::cout << "Cannot find given block" << std::endl;
        return base::config::EXIT_OK;
    }

    if (_block_hash == base::Sha256::null()) {
        _block_hash = base::Sha256::compute(base::toBytes(block));
    }

    std::cout << "Block hash " << _block_hash << '\n'
              << "\tDepth: " << block.getDepth() << '\n'
              << "\tTimestamp: " << block.getTimestamp() << '\n'
              << "\tCoinbase: " << block.getCoinbase() << '\n'
              << "\tPrevious block hash: " << block.getPrevBlockHash().toHex() << '\n'
              << "\tNumber of transactions: " << block.getTransactions().size() << std::endl;

    std::size_t tx_index = 0;
    for (const auto& tx : block.getTransactions()) {
        std::string type_message;
        if (tx.getTo() == lk::Address::null()) {
            type_message = "contract creation";
        }
        else if (tx.getData().isEmpty()) {
            type_message = "transfer";
        }
        else {
            type_message = "contract call";
        }

        std::cout << "\t\tTransaction #" << ++tx_index << '\n'
                  << "\t\tType: " << type_message << '\n'
                  << "\t\tFrom: " << tx.getFrom().toString() << '\n'
                  << "\t\tTo: " << tx.getTo().toString() << '\n'
                  << "\t\tValue: " << tx.getAmount() << '\n'
                  << "\t\tFee: " << tx.getFee() << '\n'
                  << "\t\tTimestamp: " << tx.getTimestamp() << '\n'
                  << "\t\tData: " << (tx.getData().isEmpty() ? "<empty>" : base::toHex(tx.getData())) << '\n'
                  << "\t\tSignature: " << (tx.checkSign() ? "verified" : "bad signature") << std::endl;
    }
    std::cout.flush();

    return base::config::EXIT_OK;
}
