#pragma once

#include "base/time.hpp"
#include "base/utility.hpp"
#include "core/address.hpp"
#include "core/block.hpp"
#include "net/session.hpp"

#include <forward_list>
#include <memory>

namespace lk
{

// peer context requirements for handling messages
class Core;
class Host;
class PeerPoolBase;
//=======================

class Peer : public std::enable_shared_from_this<Peer>
{
  public:
    //================
    /**
     * Classes that are required for network messages handling.
     */
    struct Context
    {
        lk::Core& core;         //! for operating with blockchain
        lk::Host& host;         //! for operating with host data
        lk::PeerPoolBase& pool; //! for new peer adding and gathering all peers info
    };

    enum class State
    {
        JUST_ESTABLISHED,
        REQUESTED_BLOCKS,
        SYNCHRONISED
    };

    struct IdentityInfo
    {
        net::Endpoint endpoint;
        lk::Address address;

        static IdentityInfo deserialize(base::SerializationIArchive& ia);
        void serialize(base::SerializationOArchive& oa) const;
    };
    //================
    static std::shared_ptr<Peer> accepted(std::shared_ptr<net::Session> session, lk::Host& host, lk::Core& core);
    static std::shared_ptr<Peer> connected(std::shared_ptr<net::Session> session, lk::Host& host, lk::Core& core);
    //================
    base::Time getLastSeen() const;
    net::Endpoint getEndpoint() const;
    net::Endpoint getPublicEndpoint() const;
    bool wasConnectedTo() const noexcept;

    void setServerEndpoint(net::Endpoint endpoint);
    void setState(State state);
    State getState() const noexcept;
    //================
    const lk::Address& getAddress() const noexcept;
    //================
    IdentityInfo getInfo() const;
    bool isClosed() const;
    //================
    void addSyncBlock(lk::Block block);
    void applySyncs();
    const std::forward_list<lk::Block>& getSyncBlocks() const noexcept;
    //================
    void send(const base::Bytes& data, net::Connection::SendHandler on_send);
    void send(base::Bytes&& data, net::Connection::SendHandler on_send);
    //================
    void sendBlock(const lk::Block& block);
    void sendTransaction(const lk::Transaction& tx);
    void sendSessionEnd(std::function<void()> on_send);
    //===============
    /**
     * If the peer was accepted, it responds to it whether the acception was successful or not.
     * If the peer was connected to it waits for reply.
     */
    void startSession();

    void lookup(const lk::Address& address, std::size_t alpha, std::function<void(std::vector<IdentityInfo>)> callback);
    std::multimap<lk::Address, std::function<void(std::vector<IdentityInfo>)>>
      _lookup_callbacks; // TODO: refactor, of course
  private:
    //================
    Peer(std::shared_ptr<net::Session> session,
         bool is_connected,
         boost::asio::io_context& io_context,
         lk::PeerPoolBase& pool,
         lk::Core& core,
         lk::Host& host);
    //================
    /*
     * Tries to add peer to a peer pool, to which it is attached.
     * @return true if success, false - otherwise
     */
    bool tryAddToPool();
    //================
    /*
     * Handler of session messages.
     */
    class Handler : public net::Session::Handler
    {
      public:
        Handler(Peer& peer);

        void onReceive(const base::Bytes& bytes) override;
        void onClose() override;

      private:
        Peer& _peer;
    };
    //================

    std::shared_ptr<net::Session> _session;
    bool _is_started{ false };
    //================
    boost::asio::io_context& _io_context;
    //================
    State _state{ State::JUST_ESTABLISHED };
    std::optional<net::Endpoint> _endpoint_for_incoming_connections;
    lk::Address _address;
    //================
    std::forward_list<lk::Block> _sync_blocks;
    //================
    bool _was_connected_to; // peer was connected to or accepted?
    bool _is_attached_to_pool{ false };
    lk::PeerPoolBase& _pool;
    lk::Core& _core;
    lk::Host& _host;
    //================
    // void rejectedByPool();
    //================
};


class PeerPoolBase
{
  public:
    virtual bool tryAddPeer(std::shared_ptr<Peer> peer) = 0;
    virtual void removePeer(std::shared_ptr<Peer> peer) = 0;
    virtual void removePeer(Peer* peer) = 0;

    virtual void forEachPeer(std::function<void(const Peer&)> f) const = 0;
    virtual void forEachPeer(std::function<void(Peer&)> f) = 0;

    virtual void broadcast(const base::Bytes& bytes) = 0;

    virtual std::vector<Peer::IdentityInfo> lookup(const lk::Address& address, std::size_t alpha) = 0;

    virtual std::vector<Peer::IdentityInfo> allPeersInfo() const = 0;
};

}