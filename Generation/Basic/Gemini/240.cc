#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/seq-ts-header.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/ipv4-address.h"
#include "ns3/inet-socket-address.h"
#include "ns3/ipv4.h"

// NS_LOG_COMPONENT_DEFINE is used to enable logging for components.
// For the final output, we don't need them to be enabled by default.
// NS_LOG_COMPONENT_DEFINE("MulticastWifiSimulation");

/*
 * Custom Multicast Sender Application
 */
class MulticastSender : public ns3::Application
{
public:
    static ns3::TypeId GetTypeId();
    MulticastSender();
    virtual ~MulticastSender();

    void SetRemote(ns3::Ipv4Address ip, uint16_t port);
    void SetPacketSize(uint32_t size);
    void SetNPackets(uint32_t nPackets);
    void SetInterval(ns3::Time interval);
    void SetSocketType(ns3::TypeId tid);

protected:
    virtual void DoInitialize();
    virtual void DoDispose();

private:
    virtual void StartApplication();
    virtual void StopApplication();

    void SendPacket();

    ns3::TypeId m_tid;
    ns3::Ptr<ns3::Socket> m_socket;
    ns3::Ipv4Address m_peerAddress;
    uint16_t m_peerPort;
    uint32_t m_packetSize;
    uint32_t m_nPackets;
    ns3::Time m_interval;
    uint32_t m_packetsSent;
};

ns3::TypeId MulticastSender::GetTypeId()
{
    static ns3::TypeId tid = ns3::TypeId("MulticastSender")
                                 .SetParent<ns3::Application>()
                                 .SetGroupName("Applications")
                                 .AddConstructor<MulticastSender>()
                                 .AddAttribute("RemoteAddress",
                                               "The destination Address of the outbound packets",
                                               ns3::Ipv4AddressValue(),
                                               ns3::MakeIpv4AddressAccessor(&MulticastSender::m_peerAddress),
                                               ns3::MakeIpv4AddressChecker())
                                 .AddAttribute("RemotePort",
                                               "The destination Port of the outbound packets",
                                               ns3::UintegerValue(0),
                                               ns3::MakeUintegerAccessor(&MulticastSender::m_peerPort),
                                               ns3::MakeUintegerChecker<uint16_t>())
                                 .AddAttribute("PacketSize",
                                               "The size of packets sent in bytes",
                                               ns3::UintegerValue(1024),
                                               ns3::MakeUintegerAccessor(&MulticastSender::m_packetSize),
                                               ns3::MakeUintegerChecker<uint32_t>())
                                 .AddAttribute("Interval",
                                               "The time to wait between packets",
                                               ns3::TimeValue(ns3::Seconds(1.0)),
                                               ns3::MakeTimeAccessor(&MulticastSender::m_interval),
                                               ns3::MakeTimeChecker())
                                 .AddAttribute("MaxPackets",
                                               "The maximum number of packets to send",
                                               ns3::UintegerValue(100),
                                               ns3::MakeUintegerAccessor(&MulticastSender::m_nPackets),
                                               ns3::MakeUintegerChecker<uint32_t>())
                                 .AddAttribute("SocketType",
                                               "The type of socket to create",
                                               ns3::TypeIdValue(ns3::UdpSocketFactory::GetTypeId()),
                                               ns3::MakeTypeIdAccessor(&MulticastSender::m_tid),
                                               ns3::MakeTypeIdChecker());
    return tid;
}

MulticastSender::MulticastSender() : m_socket(0),
                                     m_peerAddress(),
                                     m_peerPort(0),
                                     m_packetSize(0),
                                     m_nPackets(0),
                                     m_interval(),
                                     m_packetsSent(0)
{
}

MulticastSender::~MulticastSender()
{
}

void MulticastSender::SetRemote(ns3::Ipv4Address ip, uint16_t port)
{
    m_peerAddress = ip;
    m_peerPort = port;
}

void MulticastSender::SetPacketSize(uint32_t size)
{
    m_packetSize = size;
}

void MulticastSender::SetNPackets(uint32_t nPackets)
{
    m_nPackets = nPackets;
}

void MulticastSender::SetInterval(ns3::Time interval)
{
    m_interval = interval;
}

void MulticastSender::SetSocketType(ns3::TypeId tid)
{
    m_tid = tid;
}

void MulticastSender::DoInitialize()
{
    ns3::Application::DoInitialize();
}

void MulticastSender::DoDispose()
{
    m_socket = 0;
    ns3::Application::DoDispose();
}

void MulticastSender::StartApplication()
{
    if (!m_socket)
    {
        m_socket = ns3::Socket::CreateSocket(GetNode(), m_tid);
        if (m_socket->Bind() == -1) // Bind to any available port
        {
            NS_FATAL_ERROR("Failed to bind socket");
        }
    }
    m_socket->SetAllowBroadcast(true); // Enables multicast sending

    m_packetsSent = 0;
    ns3::Simulator::Schedule(ns3::Seconds(0.0), &MulticastSender::SendPacket, this);
}

void MulticastSender::StopApplication()
{
    if (m_socket)
    {
        m_socket->Close();
        m_socket = 0;
    }
}

void MulticastSender::SendPacket()
{
    if (m_packetsSent < m_nPackets)
    {
        ns3::SeqTsHeader header;
        header.SetSeq(m_packetsSent);
        header.SetTs(ns3::Simulator::Now());

        ns3::Ptr<ns3::Packet> packet = ns3::Create<ns3::Packet>(m_packetSize - header.GetSerializedSize());
        packet->AddHeader(header);

        ns3::Address dest = ns3::InetSocketAddress(m_peerAddress, m_peerPort);
        int ret = m_socket->SendTo(packet, 0, dest);
        if (ret == -1)
        {
            // Log a warning if sending fails, but don't stop the simulation.
            // ns3::errnoGet() provides the error code from the last system call.
            // ns3::strerror() converts it to a human-readable string.
            // This is primarily for debugging, can be removed for final output.
            // NS_LOG_WARN("Failed to send packet " << m_packetsSent << ": " << ns3::strerror(ns3::errnoGet()));
        }
        else
        {
            std::cout << "Sender Node " << GetNode()->GetId() << " Sent " << m_packetSize << " bytes to " << m_peerAddress << ":" << m_peerPort
                      << " seq=" << m_packetsSent << " at " << ns3::Simulator::Now().GetSeconds() << "s" << std::endl;
        }

        m_packetsSent++;
        ns3::Simulator::Schedule(m_interval, &MulticastSender::SendPacket, this);
    }
    else
    {
        StopApplication();
    }
}

/*
 * Custom Multicast Receiver Application
 */
class MulticastReceiver : public ns3::Application
{
public:
    static ns3::TypeId GetTypeId();
    MulticastReceiver();
    virtual ~MulticastReceiver();

    void SetRemote(ns3::Ipv4Address ip, uint16_t port);

protected:
    virtual void DoInitialize();
    virtual void DoDispose();

private:
    virtual void StartApplication();
    virtual void StopApplication();

    void HandleRead(ns3::Ptr<ns3::Socket> socket);

    ns3::Ptr<ns3::Socket> m_socket;
    ns3::Ipv4Address m_groupAddress;
    uint16_t m_port;
};

ns3::TypeId MulticastReceiver::GetTypeId()
{
    static ns3::TypeId tid = ns3::TypeId("MulticastReceiver")
                                 .SetParent<ns3::Application>()
                                 .SetGroupName("Applications")
                                 .AddConstructor<MulticastReceiver>()
                                 .AddAttribute("RemoteAddress",
                                               "The destination Address of the outbound packets",
                                               ns3::Ipv4AddressValue(),
                                               ns3::MakeIpv4AddressAccessor(&MulticastReceiver::m_groupAddress),
                                               ns3::MakeIpv4AddressChecker())
                                 .AddAttribute("RemotePort",
                                               "The destination Port of the outbound packets",
                                               ns3::UintegerValue(0),
                                               ns3::MakeUintegerAccessor(&MulticastReceiver::m_port),
                                               ns3::MakeUintegerChecker<uint16_t>());
    return tid;
}

MulticastReceiver::MulticastReceiver() : m_socket(0),
                                         m_groupAddress(),
                                         m_port(0)
{
}

MulticastReceiver::~MulticastReceiver()
{
}

void MulticastReceiver::SetRemote(ns3::Ipv4Address ip, uint16_t port)
{
    m_groupAddress = ip;
    m_port = port;
}

void MulticastReceiver::DoInitialize()
{
    ns3::Application::DoInitialize();
}

void MulticastReceiver::DoDispose()
{
    m_socket = 0;
    ns3::Application::DoDispose();
}

void MulticastReceiver::StartApplication()
{
    if (!m_socket)
    {
        m_socket = ns3::Socket::CreateSocket(GetNode(), ns3::UdpSocketFactory::GetTypeId());
        ns3::InetSocketAddress local = ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), m_port);
        if (m_socket->Bind(local) == -1)
        {
            NS_FATAL_ERROR("Failed to bind socket");
        }

        m_socket->SetAllowBroadcast(true); // This enables multicast reception for UDP sockets
        m_socket->SetRecvCallback(MakeCallback(&MulticastReceiver::HandleRead, this));
    }
}

void MulticastReceiver::StopApplication()
{
    if (m_socket)
    {
        m_socket->Close();
        m_socket = 0;
    }
}

void MulticastReceiver::HandleRead(ns3::Ptr<ns3::Socket> socket)
{
    ns3::Ptr<ns3::Packet> packet;
    ns3::Address from;
    ns3::Address localAddress;
    while ((packet = socket->RecvFrom(from, 0, localAddress)))
    {
        if (packet->GetSize() > 0)
        {
            ns3::SeqTsHeader header;
            packet->RemoveHeader(header);

            ns3::InetSocketAddress fromAddr = ns3::InetSocketAddress::ConvertFrom(from);
            ns3::InetSocketAddress localAddr = ns3::InetSocketAddress::ConvertFrom(localAddress);

            std::cout << "Receiver Node " << GetNode()->GetId() << " Received " << packet->GetSize() << " bytes from "
                      << fromAddr.GetIpv4() << ":" << fromAddr.GetPort()
                      << " to " << localAddr.GetIpv4() << ":" << localAddr.GetPort()
                      << " seq=" << header.GetSeq() << " ts=" << header.GetTs().GetSeconds()
                      << " at " << ns3::Simulator::Now().GetSeconds() << "s" << std::endl;
        }
    }
}

int main(int argc, char *argv[])
{
    // Enable logging for debugging (optional)
    // ns3::LogComponentEnable("MulticastSenderApp", ns3::LOG_LEVEL_INFO);
    // ns3::LogComponentEnable("MulticastReceiverApp", ns3::LOG_LEVEL_INFO);

    // Create nodes: 1 sender + 3 receivers
    ns3::NodeContainer nodes;
    nodes.Create(4);

    // Mobility: Place nodes in a line
    ns3::MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", ns3::DoubleValue(0.0),
                                  "MinY", ns3::DoubleValue(0.0),
                                  "DeltaX", ns3::DoubleValue(10.0), // 10 meters separation
                                  "DeltaY", ns3::DoubleValue(0.0),
                                  "GridWidth", ns3::UintegerValue(4),
                                  "LayoutType", ns3::StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Wi-Fi Configuration
    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_STANDARD_80211n_5GHZ); // Using 802.11n in 5GHz band

    ns3::YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::FriisPropagationLossModel", "Frequency", ns3::DoubleValue(5.180e9)); // 5.18 GHz (channel 36)

    ns3::YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    phy.SetErrorRateModel("ns3::NistErrorRateModel");

    ns3::NqmacWifiMacHelper mac = ns3::NqmacWifiMacHelper::Default();
    mac.SetType("ns3::AdhocWifiMac"); // Ad-hoc (IBSS) mode

    ns3::NetDeviceContainer devices;
    devices = wifi.Install(phy, mac, nodes);

    // Internet Stack
    ns3::InternetStackHelper internet;
    internet.Install(nodes);

    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Multicast Address and Port
    ns3::Ipv4Address multicastAddress("224.1.2.3");
    uint16_t multicastPort = 9; // Echo port is often used for simple tests

    // Sender Setup
    ns3::Ptr<ns3::Node> senderNode = nodes.Get(0);
    ns3::Ptr<MulticastSender> senderApp = ns3::CreateObject<MulticastSender>();
    senderApp->SetSocketType(ns3::UdpSocketFactory::GetTypeId());
    senderApp->SetRemote(multicastAddress, multicastPort);
    senderApp->SetPacketSize(1024); // 1KB packets
    senderApp->SetNPackets(100);    // Send 100 packets
    senderApp->SetInterval(ns3::Seconds(0.1)); // 1 packet every 0.1 seconds
    senderNode->AddApplication(senderApp);
    senderApp->SetStartTime(ns3::Seconds(1.0)); // Start sending at 1 second
    senderApp->SetStopTime(ns3::Seconds(11.0)); // Stop at 11 seconds (1s start + 100 packets * 0.1s/packet = 11s)

    // Receivers Setup
    for (uint32_t i = 1; i < nodes.GetN(); ++i)
    {
        ns3::Ptr<ns3::Node> receiverNode = nodes.Get(i);
        ns3::Ptr<MulticastReceiver> receiverApp = ns3::CreateObject<MulticastReceiver>();
        receiverApp->SetRemote(multicastAddress, multicastPort);
        receiverNode->AddApplication(receiverApp);
        receiverApp->SetStartTime(ns3::Seconds(0.0)); // Start receiving immediately
        receiverApp->SetStopTime(ns3::Seconds(12.0)); // Stop after sender finishes

        // Crucially, join the multicast group on the specific interface
        ns3::Ptr<ns3::Ipv4> ipv4Protocol = receiverNode->GetObject<ns3::Ipv4>();
        // Get the interface index for the Wi-Fi device on this receiver node
        uint32_t ifIndex = ipv4Protocol->GetInterfaceForDevice(devices.Get(i));
        // Add a multicast route to join the group
        // Ipv4Address::GetAny() means we accept packets from any source to this group
        ipv4Protocol->AddMulticastRoute(ns3::Ipv4Address::GetAny(), multicastAddress, ifIndex);
    }
    
    // Enable PCAP tracing (optional)
    phy.EnablePcap("multicast-wifi", devices);

    // Set a global stop time for the simulation
    ns3::Simulator::Stop(ns3::Seconds(12.0));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}