#pragma once

#include <P2P/IPAddress.h>
#include <P2P/Capabilities.h>
#include <P2P/SocketAddress.h>

#include <string>
#include <stdint.h>
#include <ctime>
#include <atomic>
#include <chrono>

class Peer
{
public:
	//
	// Constructors
	//
	Peer(const SocketAddress& socketAddress)
		: m_socketAddress(socketAddress), m_version(0), m_capabilities(Capabilities(Capabilities::UNKNOWN)), m_userAgent(""), m_lastContactTime(0)
	{

	}
	Peer(const SocketAddress& socketAddress, const uint32_t version, const Capabilities& capabilities, const std::string& userAgent)
		: m_socketAddress(socketAddress), m_version(version), m_capabilities(capabilities), m_userAgent(userAgent), m_lastContactTime(0)
	{

	}
	Peer(const SocketAddress& socketAddress, const uint32_t version, const Capabilities& capabilities, const std::string& userAgent, const std::time_t lastContactTime)
		: m_socketAddress(socketAddress), m_version(version), m_capabilities(capabilities), m_userAgent(userAgent), m_lastContactTime(lastContactTime)
	{

	}
	Peer(const Peer& other)
		: m_socketAddress(other.m_socketAddress), m_version(other.m_version), m_capabilities(other.m_capabilities.load()), m_userAgent(other.m_userAgent), m_lastContactTime(other.m_lastContactTime.load())
	{

	}
	Peer(Peer&& other) = default;
	Peer() = default;

	//
	// Destructor
	//
	~Peer() = default;

	//
	// Operators
	//
	Peer& operator=(const Peer& other) = default;
	Peer& operator=(Peer&& other) = default;

	//
	// Setters
	//
	inline void UpdateVersion(const uint32_t version) { m_version = version; }
	inline void UpdateCapabilities(const Capabilities& capabilities) { m_capabilities = capabilities; }
	inline void UpdateLastContactTime() const { m_lastContactTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()); }
	inline void UpdateUserAgent(const std::string& userAgent) { m_userAgent = userAgent; }

	//
	// Getters
	//
	inline const SocketAddress& GetSocketAddress() const { return m_socketAddress; }
	inline const IPAddress& GetIPAddress() const { return m_socketAddress.GetIPAddress(); }
	inline uint16_t GetPortNumber() const { return m_socketAddress.GetPortNumber(); }
	inline uint32_t GetVersion() const { return m_version; }
	inline Capabilities GetCapabilities() const { return m_capabilities; }
	inline const std::string& GetUserAgent() const { return m_userAgent; }
	inline std::time_t GetLastContactTime() const { return m_lastContactTime; }

	//
	// Serialization/Deserialization
	//
	void Serialize(Serializer& serializer) const
	{
		m_socketAddress.Serialize(serializer);
		serializer.Append<uint32_t>(m_version);
		m_capabilities.load().Serialize(serializer);
		serializer.AppendVarStr(m_userAgent);
		serializer.Append<int64_t>(m_lastContactTime.load());
	}

	static Peer Deserialize(ByteBuffer& byteBuffer)
	{
		SocketAddress socketAddress = SocketAddress::Deserialize(byteBuffer);
		uint32_t version = byteBuffer.ReadU32();
		Capabilities capabilities = Capabilities::Deserialize(byteBuffer);
		std::string userAgent = byteBuffer.ReadVarStr();
		std::time_t lastContactTIme = (std::time_t)byteBuffer.Read64();

		return Peer(socketAddress, version, capabilities, userAgent, lastContactTIme);
	}

private:
	const SocketAddress m_socketAddress;
	uint32_t m_version;
	std::atomic<Capabilities> m_capabilities;
	std::string m_userAgent;
	mutable std::atomic<std::time_t> m_lastContactTime;
};