#include "base/error.hpp"

#include "transaction.hpp"

namespace bc
{

Sign::Sign(base::RsaPublicKey sender_public_key, base::Bytes rsa_encrypted_hash)
    : _data{Data{std::move(sender_public_key), std::move(rsa_encrypted_hash)}}
{}


bool Sign::isNull() const noexcept
{
    return !_data.has_value();
}


const base::RsaPublicKey& Sign::getPublicKey() const
{
    if(isNull()) {
        RAISE_ERROR(base::LogicError, "attemping to get on null bc::Sign");
    }
    return _data->sender_public_key;
}


const base::Bytes& Sign::getRsaEncryptedHash() const
{
    if(isNull()) {
        RAISE_ERROR(base::LogicError, "attemping to get on null bc::Sign");
    }
    return _data->rsa_encrypted_hash;
}


void Sign::serialize(base::SerializationOArchive& oa) const
{
    if(!_data) {
        oa.serialize(base::Byte{false});
    }
    else {
        oa.serialize(base::Byte{true});
        oa.serialize(_data->sender_public_key);
        oa.serialize(_data->rsa_encrypted_hash);
    }
}


Sign Sign::deserialize(base::SerializationIArchive& ia)
{
    auto flag = ia.deserialize<base::Byte>();
    if(flag) {
        auto sender_rsa_public_key = base::RsaPublicKey::deserialize(ia);
        auto rsa_encrypted_hash = ia.deserialize<base::Bytes>();
        return Sign{std::move(sender_rsa_public_key), std::move(rsa_encrypted_hash)};
    }
    else {
        return Sign{};
    }
}


Transaction::Transaction(bc::Address from, bc::Address to, bc::Balance amount, bc::Balance fee, base::Time timestamp,
    base::Sha256 code_hash, bc::Sign sign)
    : _from{std::move(from)}, _to{std::move(to)}, _amount{amount}, _fee{fee}, _timestamp{timestamp},
      _code_hash{std::move(code_hash)}, _sign{std::move(sign)}
{
    if(_amount == 0) {
        RAISE_ERROR(base::LogicError, "Transaction cannot contain amount equal to 0");
    }
}


const bc::Address& Transaction::getFrom() const noexcept
{
    return _from;
}


const bc::Address& Transaction::getTo() const noexcept
{
    return _to;
}


const bc::Balance& Transaction::getAmount() const noexcept
{
    return _amount;
}


const base::Time& Transaction::getTimestamp() const noexcept
{
    return _timestamp;
}


const bc::Balance& Transaction::getFee() const noexcept
{
    return _fee;
}


const base::Sha256& Transaction::getCodeHash() const noexcept
{
    return _code_hash;
}


bool Transaction::operator==(const Transaction& other) const
{
    return _amount == other._amount && _from == other._from && _to == other._to && _timestamp == other._timestamp &&
        _fee == other._fee;
}


bool Transaction::operator!=(const Transaction& other) const
{
    return !(*this == other);
}


void Transaction::sign(base::RsaPublicKey pub, const base::RsaPrivateKey& priv)
{
    auto hash = hashOfTxData();
    // TODO: do a better elliptic curve signature
    base::Bytes rsa_encrypted_hash = priv.encrypt(hash.getBytes());
    _sign = Sign{std::move(pub), rsa_encrypted_hash};
}


bool Transaction::checkSign() const
{
    if(_sign.isNull()) {
        return false;
    }
    else {
        const auto& pub = _sign.getPublicKey();
        const auto& enc_hash = _sign.getRsaEncryptedHash();
        auto derived_addr = bc::Address::fromPublicKey(pub);
        if(_from != derived_addr) {
            return false;
        }
        auto valid_hash = hashOfTxData();
        if(pub.decrypt(enc_hash) == valid_hash.getBytes()) {
            return true;
        }
        return false;
    }
}


const Sign& Transaction::getSign() const noexcept
{
    return _sign;
}


void Transaction::serializeHeader(base::SerializationOArchive& oa) const
{
    oa.serialize(_from);
    oa.serialize(_to);
    oa.serialize(_amount);
    oa.serialize(_fee);
    oa.serialize(_timestamp);
}


base::Sha256 Transaction::hashOfTxData() const
{
    base::SerializationOArchive oa;
    serializeHeader(oa);
    return base::Sha256::compute(std::move(oa).getBytes());
}


Transaction Transaction::deserialize(base::SerializationIArchive& ia)
{
    auto from = ia.deserialize<bc::Address>();
    auto to = ia.deserialize<bc::Address>();
    auto balance = ia.deserialize<bc::Balance>();
    auto fee = ia.deserialize<bc::Balance>();
    auto timestamp = ia.deserialize<base::Time>();
    auto code_hash = ia.deserialize<base::Sha256>();
    auto sign = ia.deserialize<bc::Sign>();
    return {std::move(from), std::move(to), balance, fee, timestamp, std::move(code_hash), std::move(sign)};
}


void Transaction::serialize(base::SerializationOArchive& oa) const
{
    serializeHeader(oa);
    oa.serialize(_sign);
}


std::ostream& operator<<(std::ostream& os, const Transaction& tx)
{
    return os << "from: " << tx.getFrom() << " to: " << tx.getTo() << " amount: " << tx.getAmount()
              << " fee: " << tx.getFee() << " timestamp: " << tx.getTimestamp();
}


void TransactionBuilder::setFrom(bc::Address from)
{
    _from = std::move(from);
}


void TransactionBuilder::setTo(bc::Address to)
{
    _to = std::move(to);
}


void TransactionBuilder::setAmount(bc::Balance amount)
{
    _amount = amount;
}


void TransactionBuilder::setTimestamp(base::Time timestamp)
{
    _timestamp = timestamp;
}


void TransactionBuilder::setFee(bc::Balance fee)
{
    _fee = fee;
}


void TransactionBuilder::setCodeHash(base::Sha256 code_hash)
{
    _code_hash = std::move(code_hash);
}


Transaction TransactionBuilder::build() const&
{
    ASSERT(_from);
    ASSERT(_to);
    ASSERT(_amount);
    ASSERT(_fee);
    ASSERT(_timestamp);
    ASSERT(_code_hash);
    return {*_from, *_to, *_amount, *_fee, *_timestamp, *_code_hash};
}


Transaction TransactionBuilder::build() &&
{
    ASSERT(_from);
    ASSERT(_to);
    ASSERT(_amount);
    ASSERT(_fee);
    ASSERT(_timestamp);
    ASSERT(_code_hash);
    return {std::move(*_from), std::move(*_to), std::move(*_amount), std::move(*_fee), std::move(*_timestamp),
        std::move(*_code_hash)};
}


} // namespace bc
