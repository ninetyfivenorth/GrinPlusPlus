#include "ConnectionFactory.h"

#include "../Messages/HandMessage.h"
#include "../Messages/ShakeMessage.h"
#include "../MessageRetriever.h"
#include "../MessageSender.h"
#include "../SocketHelper.h"
#include "../ConnectionManager.h"
#include "PeerManager.h"

#include <ctime>
#include <WS2tcpip.h>
#include <VectorUtil.h>
#include <cstdlib>

ConnectionFactory::ConnectionFactory(const Config& config, ConnectionManager& connectionManager, PeerManager& peerManager, IBlockChainServer& blockChainServer)
	: m_config(config), m_connectionManager(connectionManager), m_peerManager(peerManager), m_blockChainServer(blockChainServer)
{

}

Connection* ConnectionFactory::CreateConnection(Peer& peer, const EDirection direction, const std::optional<SOCKET>& socketOpt) const
{
	std::optional<SOCKET> socketOptional = socketOpt;
	if (direction == EDirection::OUTBOUND)
	{
		socketOptional = SocketHelper::Connect(peer.GetIPAddress(), peer.GetPortNumber());
	}

	if (socketOptional.has_value())
	{
		SOCKET socket = socketOptional.value();
		Connection* pConnection = PerformHandshake(socket, peer, direction);
		if (pConnection != nullptr)
		{
			// This will start the message processing loop.
			pConnection->Connect();

			return pConnection;
		}

		closesocket(socket);
	}

	return nullptr;
}

Connection* ConnectionFactory::PerformHandshake(SOCKET connection, Peer& peer, const EDirection direction) const
{
	ConnectedPeer connectedPeer(connection, peer, direction);
	if (direction == EDirection::OUTBOUND)
	{
		// Send Hand Message
		const bool bHandMessageSent = TransmitHandMessage(connectedPeer);
		if (bHandMessageSent)
		{
			// Get Shake Message
			std::unique_ptr<RawMessage> receivedShakeMessage = MessageRetriever(m_config).RetrieveMessage(connectedPeer, MessageRetriever::BLOCKING);

			if (receivedShakeMessage.get() != nullptr && receivedShakeMessage->GetMessageHeader().GetMessageType() == MessageTypes::Shake)
			{
				ByteBuffer byteBuffer(receivedShakeMessage->GetPayload());
				const ShakeMessage shakeMessage = ShakeMessage::Deserialize(byteBuffer);

				connectedPeer.UpdateVersion(shakeMessage.GetVersion());
				connectedPeer.UpdateCapabilities(shakeMessage.GetCapabilities());
				connectedPeer.UpdateUserAgent(shakeMessage.GetUserAgent());
				connectedPeer.UpdateTotals(shakeMessage.GetTotalDifficulty(), 0);

				return new Connection(m_nextId++, m_config, m_connectionManager, m_peerManager, m_blockChainServer, connectedPeer);
			}
		}
	}
	else
	{
		// Get Hand Message
		std::unique_ptr<RawMessage> receivedHandMessage = MessageRetriever(m_config).RetrieveMessage(connectedPeer, MessageRetriever::BLOCKING);
		if (receivedHandMessage != nullptr && receivedHandMessage->GetMessageHeader().GetMessageType() == MessageTypes::Hand)
		{
			ByteBuffer byteBuffer(receivedHandMessage->GetPayload());
			const HandMessage handMessage = HandMessage::Deserialize(byteBuffer);

			connectedPeer.UpdateVersion(handMessage.GetVersion());
			connectedPeer.UpdateCapabilities(handMessage.GetCapabilities());
			connectedPeer.UpdateUserAgent(handMessage.GetUserAgent());
			connectedPeer.UpdateTotals(handMessage.GetTotalDifficulty(), 0);

			// Send Shake Message
			if (TransmitShakeMessage(connectedPeer))
			{
				return new Connection(m_nextId++, m_config, m_connectionManager, m_peerManager, m_blockChainServer, connectedPeer);
			}
		}
	}


	return nullptr;
}

bool ConnectionFactory::TransmitHandMessage(ConnectedPeer& connectedPeer) const
{
	const unsigned char localhostIP[4] = { 0x7F, 0x00, 0x00, 0x01 };
	std::vector<unsigned char> address = VectorUtil::MakeVector<unsigned char, 4>(localhostIP);
	const IPAddress localHostIP(EAddressFamily::IPv4, address);

	std::optional<uint16_t> portOptional = SocketHelper::GetPort(connectedPeer.GetSocket());
	const uint16_t portNumber = portOptional.has_value() ? portOptional.value() : m_config.GetEnvironment().GetP2PPort();

	const uint32_t version = P2P::PROTOCOL_VERSION;
	const Capabilities capabilities(Capabilities::FAST_SYNC_NODE); // LIGHT_CLIENT: Read P2P Config once light-clients are supported
	const uint64_t nonce = rand();
	Hash hash = m_config.GetEnvironment().GetGenesisHash();
	const uint64_t totalDifficulty = m_blockChainServer.GetTotalDifficulty(EChainType::CONFIRMED);
	SocketAddress senderAddress(localHostIP, m_config.GetEnvironment().GetP2PPort());
	SocketAddress receiverAddress(localHostIP, portNumber);
	const std::string& userAgent = P2P::USER_AGENT;
	const HandMessage handMessage(version, capabilities, nonce, std::move(hash), totalDifficulty, std::move(senderAddress), std::move(receiverAddress), userAgent);

	return MessageSender(m_config).Send(connectedPeer, handMessage);
}

bool ConnectionFactory::TransmitShakeMessage(ConnectedPeer& connectedPeer) const
{
	std::optional<uint16_t> portOptional = SocketHelper::GetPort(connectedPeer.GetSocket());
	const uint16_t portNumber = portOptional.has_value() ? portOptional.value() : m_config.GetEnvironment().GetP2PPort();

	const uint32_t version = P2P::PROTOCOL_VERSION;
	const Capabilities capabilities(Capabilities::FAST_SYNC_NODE); // LIGHT_CLIENT: Read P2P Config once light-clients are supported
	const uint64_t nonce = rand();
	Hash hash = m_config.GetEnvironment().GetGenesisHash();
	const uint64_t totalDifficulty = m_blockChainServer.GetTotalDifficulty(EChainType::CONFIRMED);
	const std::string& userAgent = P2P::USER_AGENT;
	const ShakeMessage shakeMessage(version, capabilities, std::move(hash), totalDifficulty, userAgent);

	return MessageSender(m_config).Send(connectedPeer, shakeMessage);
}