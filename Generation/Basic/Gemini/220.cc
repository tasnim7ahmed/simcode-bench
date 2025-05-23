#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/udp-socket.h"

// Do not use namespace ns3; to avoid polluting the global namespace.
// Instead, explicitly qualify ns3 types or use using declarations for specific types.

// Logging setup
NS_LOG_COMPONENT_DEFINE ("CustomUdpRawSocketSim");

// --- Custom UDP Server Application ---
class CustomUdpServerApp : public ns3::Application
{
public:
    static ns3::TypeId GetTypeId();
    CustomUdpServerApp();
    virtual ~CustomUdpServerApp();

    void SetPort(uint16_t port);

protected:
    virtual void StartApplication();
    virtual void StopApplication();

private:
    void HandleRead(ns3::Ptr<ns3::Socket> socket);

    ns3::Ptr<ns3::Socket> m_socket;
    uint16_t m_port;
};

NS_OBJECT_ENSURE_REGISTERED(CustomUdpServerApp);

ns3::TypeId CustomUdpServerApp::GetTypeId()
{
    static ns3::TypeId tid = ns3::TypeId("CustomUdpServerApp")
                                 .SetParent<ns3::Application>()
                                 .SetGroupName("Applications")
                                 .AddConstructor<CustomUdpServerApp>()
                                 .AddAttribute("Port",
                                               "Port on which to listen for incoming packets.",
                                               ns3::UintegerValue(9),
                                               ns3::MakeUintegerAccessor(&CustomUdpServerApp::m_port),
                                               ns3::MakeUintegerChecker<uint16_t>());
    return tid;
}

CustomUdpServerApp::CustomUdpServerApp() : m_port(0)
{
    NS_LOG_FUNCTION(this);
}

CustomUdpServerApp::~CustomUdpServerApp()
{
    NS_LOG_FUNCTION(this);
}

void CustomUdpServerApp::SetPort(uint16_t port)
{
    m_port = port;
}

void CustomUdpServerApp::StartApplication()
{
    NS_LOG_FUNCTION(this);

    // Create a UDP socket
    m_socket = ns3::Socket::CreateSocket(GetNode(), ns3::UdpSocketFactory::GetTypeId());

    ns3::InetSocketAddress localAddress = ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), m_port);
    if (m_socket->Bind(localAddress) == -1)
    {
        NS_FATAL_ERROR("Failed to bind socket to port " << m_port);
    }
    m_socket->SetRecvCallback(ns3::MakeCallback(&CustomUdpServerApp::HandleRead, this));
    NS_LOG_INFO("Custom UDP Server listening on port " << m_port);
}

void CustomUdpServerApp::StopApplication()
{
    NS_LOG_FUNCTION(this);
    if (m_socket)
    {
        m_socket->Close();
        m_socket = nullptr;
    }
}

void CustomUdpServerApp::HandleRead(ns3::Ptr<ns3::Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    ns3::Ptr<ns3::Packet> packet;
    ns3::Address fromAddress;
    while ((packet = socket->RecvFrom(fromAddress)))
    {
        if (ns3::InetSocketAddress::Is
            (fromAddress))
        {
            ns3::InetSocketAddress inetAddress = ns3::InetSocketAddress::ConvertFrom(fromAddress);
            NS_LOG_INFO("At time " << ns3::Simulator::Now().GetSeconds() << "s: Server received "
                                   << packet->GetSize() << " bytes from "
                                   << inetAddress.GetIpv4() << " port "
                                   << inetAddress.GetPort());
        }
        else
        {
            NS_LOG_INFO("At time " << ns3::Simulator::Now().GetSeconds() << "s: Server received "
                                   << packet->GetSize() << " bytes from unknown source.");
        }
    }
}

// --- Custom UDP Client Application ---
class CustomUdpClientApp : public ns3::Application
{
public:
    static ns3::TypeId GetTypeId();
    CustomUdpClientApp();
    virtual ~CustomUdpClientApp();

    void SetRemote(ns3::Address ip, uint16_t port);
    void SetPacketSize(uint32_t size);
    void SetSendInterval(ns3::Time interval);
    void SetMaxPackets(uint32_t maxPackets);

protected:
    virtual void StartApplication();
    virtual void StopApplication();

private:
    void SendPacket();

    ns3::Ptr<ns3::Socket> m_socket;
    ns3::Address m_peerAddress;
    uint32_t m_packetSize;
    ns3::Time m_sendInterval;
    uint32_t m_packetsSent;
    uint32_t m_maxPackets;
    ns3::EventId m_sendEvent;
};

NS_OBJECT_ENSURE_REGISTERED(CustomUdpClientApp);

ns3::TypeId CustomUdpClientApp::GetTypeId()
{
    static ns3::TypeId tid = ns3::TypeId("CustomUdpClientApp")
                                 .SetParent<ns3::Application>()
                                 .SetGroupName("Applications")
                                 .AddConstructor<CustomUdpClientApp>()
                                 .AddAttribute("RemoteAddress",
                                               "The destination IP address of the remote server.",
                                               ns3::AddressValue(),
                                               ns3::MakeAddressAccessor(&CustomUdpClientApp::m_peerAddress),
                                               ns3::MakeAddressChecker())
                                 .AddAttribute("RemotePort",
                                               "The destination port of the remote server.",
                                               ns3::UintegerValue(9),
                                               ns3::MakeUintegerAccessor(&CustomUdpClientApp::m_peerAddress),
                                               ns3::MakeUintegerChecker<uint16_t>())
                                 .AddAttribute("PacketSize",
                                               "Size of packets to send.",
                                               ns3::UintegerValue(1024),
                                               ns3::MakeUintegerAccessor(&CustomUdpClientApp::m_packetSize),
                                               ns3::MakeUintegerChecker<uint32_t>())
                                 .AddAttribute("SendInterval",
                                               "Time interval between packets.",
                                               ns3::TimeValue(ns3::Seconds(1.0)),
                                               ns3::MakeTimeAccessor(&CustomUdpClientApp::m_sendInterval),
                                               ns3::MakeTimeChecker())
                                 .AddAttribute("MaxPackets",
                                               "Maximum number of packets to send.",
                                               ns3::UintegerValue(10),
                                               ns3::MakeUintegerAccessor(&CustomUdpClientApp::m_maxPackets),
                                               ns3::MakeUintegerChecker<uint32_t>());
    return tid;
}

CustomUdpClientApp::CustomUdpClientApp()
    : m_packetSize(0), m_packetsSent(0), m_maxPackets(0), m_sendEvent()
{
    NS_LOG_FUNCTION(this);
}

CustomUdpClientApp::~CustomUdpClientApp()
{
    NS_LOG_FUNCTION(this);
}

void CustomUdpClientApp::SetRemote(ns3::Address ip, uint16_t port)
{
    m_peerAddress = ns3::InetSocketAddress(ip, port);
}

void CustomUdpClientApp::SetPacketSize(uint32_t size)
{
    m_packetSize = size;
}

void CustomUdpClientApp::SetSendInterval(ns3::Time interval)
{
    m_sendInterval = interval;
}

void CustomUdpClientApp::SetMaxPackets(uint32_t maxPackets)
{
    m_maxPackets = maxPackets;
}

void CustomUdpClientApp::StartApplication()
{
    NS_LOG_FUNCTION(this);

    // Create a UDP socket
    m_socket = ns3::Socket::CreateSocket(GetNode(), ns3::UdpSocketFactory::GetTypeId());

    // Connect the socket to the remote address. For UDP, this just sets the
    // default destination for subsequent Send() calls.
    m_socket->Connect(m_peerAddress);

    NS_LOG_INFO("Custom UDP Client connected to " << ns3::InetSocketAddress::ConvertFrom(m_peerAddress).GetIpv4()
                                                  << " port " << ns3::InetSocketAddress::ConvertFrom(m_peerAddress).GetPort());

    m_packetsSent = 0;
    m_sendEvent = ns3::Simulator::Schedule(ns3::Seconds(0.0), &CustomUdpClientApp::SendPacket, this);
}

void CustomUdpClientApp::StopApplication()
{
    NS_LOG_FUNCTION(this);
    ns3::Simulator::Cancel(m_sendEvent);
    if (m_socket)
    {
        m_socket->Close();
        m_socket = nullptr;
    }
}

void CustomUdpClientApp::SendPacket()
{
    NS_LOG_FUNCTION(this);

    if (m_packetsSent < m_maxPackets)
    {
        ns3::Ptr<ns3::Packet> packet = ns3::Create<ns3::Packet>(m_packetSize);
        int ret = m_socket->Send(packet);
        if (ret == -1)
        {
            NS_LOG_WARN("Failed to send packet: " << GetNode()->GetId() << " to " << ns3::InetSocketAddress::ConvertFrom(m_peerAddress).GetIpv4()
                                                  << ". Error: " << m_socket->GetErrno());
        }
        else if (static_cast<uint32_t>(ret) != m_packetSize)
        {
            NS_LOG_WARN("Sent " << ret << " bytes, expected " << m_packetSize);
        }
        else
        {
            NS_LOG_INFO("At time " << ns3::Simulator::Now().GetSeconds() << "s: Client sent "
                                   << packet->GetSize() << " bytes to "
                                   << ns3::InetSocketAddress::ConvertFrom(m_peerAddress).GetIpv4() << " port "
                                   << ns3::InetSocketAddress::ConvertFrom(m_peerAddress).GetPort());
        }
        m_packetsSent++;
        m_sendEvent = ns3::Simulator::Schedule(m_sendInterval, &CustomUdpClientApp::SendPacket, this);
    }
}

// --- Main Simulation Function ---
int main(int argc, char *argv[])
{
    // Enable logging
    ns3::LogComponentEnable("CustomUdpRawSocketSim", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("CustomUdpServerApp", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("CustomUdpClientApp", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("UdpSocketImpl", ns3::LOG_LEVEL_INFO); // To see socket send/recv details

    // Command line arguments
    ns3::CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes
    ns3::NodeContainer nodes;
    nodes.Create(2); // node 0: client, node 1: server

    // Create point-to-point helper
    ns3::PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", ns3::StringValue("100Mbps"));
    p2p.SetChannelAttribute("Delay", ns3::StringValue("2ms"));

    // Install net devices
    ns3::NetDeviceContainer devices = p2p.Install(nodes);

    // Install Internet stack
    ns3::InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Get client and server IP addresses
    ns3::Ipv4Address clientIp = interfaces.GetAddress(0);
    ns3::Ipv4Address serverIp = interfaces.GetAddress(1);
    uint16_t serverPort = 9; // Standard Discard Protocol port

    // Install Server Application
    ns3::Ptr<CustomUdpServerApp> serverApp = ns3::CreateObject<CustomUdpServerApp>();
    serverApp->SetPort(serverPort);
    nodes.Get(1)->AddApplication(serverApp); // Server on node 1
    serverApp->SetStartTime(ns3::Seconds(0.0));
    serverApp->SetStopTime(ns3::Seconds(10.0));

    // Install Client Application
    ns3::Ptr<CustomUdpClientApp> clientApp = ns3::CreateObject<CustomUdpClientApp>();
    clientApp->SetRemote(serverIp, serverPort);
    clientApp->SetPacketSize(100);             // 100 bytes payload
    clientApp->SetSendInterval(ns3::Seconds(1.0)); // Send every 1 second
    clientApp->SetMaxPackets(10);              // Send 10 packets total over 10 seconds
    nodes.Get(0)->AddApplication(clientApp); // Client on node 0
    clientApp->SetStartTime(ns3::Seconds(0.0));
    clientApp->SetStopTime(ns3::Seconds(10.0));

    // Enable PCAP tracing
    p2p.EnablePcapAll("custom-udp-raw");

    // Run simulation
    ns3::Simulator::Stop(ns3::Seconds(10.0));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    NS_LOG_INFO("Simulation finished.");

    return 0;
}