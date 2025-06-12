#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiUdpCustom");

class UdpClient : public Application {
public:
    UdpClient();
    virtual ~UdpClient();

    void Setup(Address address, uint32_t packetSize, uint32_t maxPackets, Time interval);

protected:
    virtual void StartApplication(void);
    virtual void StopApplication(void);

private:
    void SendPacket(void);

    Address m_peerAddress;
    uint32_t m_packetSize;
    uint32_t m_maxPackets;
    Time m_interval;
    Socket *m_socket;
    uint32_t m_packetsSent;
    EventId m_sendEvent;
};

UdpClient::UdpClient() :
    m_peerAddress(),
    m_packetSize(0),
    m_maxPackets(0),
    m_interval(Seconds(0)),
    m_socket(0),
    m_packetsSent(0),
    m_sendEvent()
{
}

UdpClient::~UdpClient()
{
    m_socket = 0;
}

void
UdpClient::Setup(Address address, uint32_t packetSize, uint32_t maxPackets, Time interval)
{
    m_peerAddress = address;
    m_packetSize = packetSize;
    m_maxPackets = maxPackets;
    m_interval = interval;
}

void
UdpClient::StartApplication(void)
{
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    if (m_socket->Bind() == -1)
    {
        NS_FATAL_ERROR("Failed to bind socket");
    }
    m_socket->Connect(m_peerAddress);
    m_socket->SetAllowBroadcast(true);
    m_packetsSent = 0;
    SendPacket();
}

void
UdpClient::StopApplication(void)
{
    Simulator::Cancel(m_sendEvent);
    if (m_socket != 0)
    {
        m_socket->Close();
    }
}

void
UdpClient::SendPacket(void)
{
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    m_socket->Send(packet);

    if (++m_packetsSent < m_maxPackets)
    {
        m_sendEvent = Simulator::Schedule(m_interval, &UdpClient::SendPacket, this);
    }
}

class UdpServer : public Application {
public:
    UdpServer();
    virtual ~UdpServer();

    void Setup(uint16_t port);

protected:
    virtual void StartApplication(void);
    virtual void StopApplication(void);

private:
    void HandleRead(Ptr<Socket> socket);
    void HandleAccept(Ptr<Socket> socket, const Address& from);
    void HandleClose(Ptr<Socket> socket);

    uint16_t m_port;
    Socket *m_socket;
    bool m_running;
};

UdpServer::UdpServer() :
    m_port(0),
    m_socket(0),
    m_running(false)
{
}

UdpServer::~UdpServer()
{
    m_socket = 0;
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
    if (m_socket->Bind(local) == -1)
    {
        NS_FATAL_ERROR("Failed to bind socket");
    }

    m_socket->SetRecvCallback(MakeCallback(&UdpServer::HandleRead, this));
    m_running = true;
}

void
UdpServer::StopApplication(void)
{
    m_running = false;
    if (m_socket != 0)
    {
        m_socket->Close();
    }
}

void
UdpServer::HandleRead(Ptr<Socket> socket)
{
    Address from;
    Ptr<Packet> packet;
    while ((packet = socket->RecvFrom(from)))
    {
        std::cout << "Received one packet from " << InetSocketAddress::ConvertFrom(from).GetIpv4() << " port " << InetSocketAddress::ConvertFrom(from).GetPort() << " at time " << Simulator::Now().GetSeconds() << std::endl;
    }
}

void
UdpServer::HandleAccept(Ptr<Socket> socket, const Address& from)
{
    socket->SetRecvCallback(MakeCallback(&UdpServer::HandleRead, this));
}

void
UdpServer::HandleClose(Ptr<Socket> socket)
{
}

int main(int argc, char *argv[]) {
    bool verbose = false;
    uint32_t nWifi = 1;
    bool tracing = false;

    CommandLine cmd;
    cmd.AddValue("nWifi", "Number of wifi STA devices", nWifi);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);

    cmd.Parse(argc, argv);

    if (nWifi > 0) {
        nWifi = 1;
    }

    NodeContainer staNodes;
    staNodes.Create(nWifi);
    NodeContainer apNode;
    apNode.Create(1);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("ns-3-ssid");
    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, staNodes);

    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevices = wifi.Install(wifiPhy, wifiMac, apNode);

    MobilityHelper mobility;

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(staNodes);
    mobility.Install(apNode);

    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer staNodeInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apNodeInterface = address.Assign(apDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;

    UdpServer serverApp;
    serverApp.Setup(port);

    ApplicationContainer serverApps;
    serverApps.Add(serverApp.Install(apNode.Get(0)));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpClient clientApp;
    InetSocketAddress serverAddr(apNodeInterface.GetAddress(0), port);
    clientApp.Setup(serverAddr, 1024, 5, Seconds(1.0));
    ApplicationContainer clientApps;
    clientApps.Add(clientApp.Install(staNodes.Get(0)));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    if (tracing == true) {
        wifiPhy.EnablePcap("wifi-custom", apDevices.Get(0));
        wifiPhy.EnablePcap("wifi-custom", staDevices.Get(0));
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}