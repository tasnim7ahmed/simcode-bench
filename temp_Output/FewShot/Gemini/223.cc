#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocUdp");

class UdpClient : public Application {
public:
    UdpClient();
    virtual ~UdpClient();

    void Setup(Address address, uint32_t packetSize, uint32_t maxPackets, Time packetInterval);

private:
    virtual void StartApplication(void);
    virtual void StopApplication(void);

    void SendPacket(void);
    void ReceiveAck(Ptr<Socket> socket);

    Address m_peerAddress;
    uint32_t m_packetSize;
    uint32_t m_maxPackets;
    Time m_packetInterval;
    Ptr<Socket> m_socket;
    uint32_t m_packetsSent;
    EventId m_sendEvent;
};

UdpClient::UdpClient() :
    m_peerAddress(),
    m_packetSize(0),
    m_maxPackets(0),
    m_packetInterval(Seconds(0)),
    m_socket(nullptr),
    m_packetsSent(0),
    m_sendEvent()
{
}

UdpClient::~UdpClient()
{
    m_socket = nullptr;
}

void
UdpClient::Setup(Address address, uint32_t packetSize, uint32_t maxPackets, Time packetInterval)
{
    m_peerAddress = address;
    m_packetSize = packetSize;
    m_maxPackets = maxPackets;
    m_packetInterval = packetInterval;
}

void
UdpClient::StartApplication(void)
{
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->Bind();
    m_socket->Connect(m_peerAddress);
    m_socket->SetRecvCallback(MakeCallback(&UdpClient::ReceiveAck, this));
    m_packetsSent = 0;
    SendPacket();
}

void
UdpClient::StopApplication(void)
{
    Simulator::Cancel(m_sendEvent);
    if (m_socket != nullptr) {
        m_socket->Close();
    }
}

void
UdpClient::SendPacket(void)
{
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    m_socket->Send(packet);

    if (++m_packetsSent < m_maxPackets) {
        m_sendEvent = Simulator::Schedule(m_packetInterval, &UdpClient::SendPacket, this);
    }
}

void
UdpClient::ReceiveAck(Ptr<Socket> socket)
{
    Address from;
    Ptr<Packet> packet = socket->RecvFrom(from);
    NS_LOG_INFO("Received acknowledgement.");
}


class UdpServer : public Application {
public:
    UdpServer();
    virtual ~UdpServer();

    void Setup(uint16_t port);

private:
    virtual void StartApplication(void);
    virtual void StopApplication(void);

    void HandleRead(Ptr<Socket> socket);
    void SendAck(Ptr<Socket> socket, Address address);

    uint16_t m_port;
    Ptr<Socket> m_socket;
};

UdpServer::UdpServer() :
    m_port(0),
    m_socket(nullptr)
{
}

UdpServer::~UdpServer()
{
    m_socket = nullptr;
}

void
UdpServer::Setup(uint16_t port)
{
    m_port = port;
}

void
UdpServer::StartApplication(void)
{
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
    m_socket->Bind(local);
    m_socket->SetRecvCallback(MakeCallback(&UdpServer::HandleRead, this));
}

void
UdpServer::StopApplication(void)
{
    if (m_socket != nullptr) {
        m_socket->Close();
    }
}

void
UdpServer::HandleRead(Ptr<Socket> socket)
{
    Address from;
    Ptr<Packet> packet = socket->RecvFrom(from);
    uint32_t packetSize = packet->GetSize();

    NS_LOG_INFO("Received " << packetSize << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4() << " port " << InetSocketAddress::ConvertFrom(from).GetPort());
    SendAck(socket, from);
}

void
UdpServer::SendAck(Ptr<Socket> socket, Address address)
{
    Ptr<Packet> packet = Create<Packet>(0);
    socket->SendTo(packet, 0, address);
}

int main(int argc, char* argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
    wifiPhy.SetChannel(wifiChannel.Create());

    NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;

    UdpServerHelper serverHelper(port);
    ApplicationContainer serverApp = serverHelper.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    Address serverAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    UdpClientHelper clientHelper(serverAddress);
    clientHelper.SetAttribute("MaxPackets", UintegerValue(10));
    clientHelper.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}