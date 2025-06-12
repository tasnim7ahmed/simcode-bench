#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpAdHocSimulation");

class UdpServer : public Application {
public:
    UdpServer() : m_socket(nullptr), m_port(9) {}
    virtual ~UdpServer();

    static TypeId GetTypeId(void);
    void Setup(Ptr<Socket> socket, uint16_t port);

private:
    virtual void StartApplication(void);
    virtual void StopApplication(void);
    void HandleRead(Ptr<Socket> socket);

    Ptr<Socket> m_socket;
    uint16_t m_port;
};

UdpServer::~UdpServer() {
    m_socket = nullptr;
}

TypeId UdpServer::GetTypeId(void) {
    static TypeId tid = TypeId("UdpServer")
                            .SetParent<Application>()
                            .SetGroupName("Applications")
                            .AddConstructor<UdpServer>();
    return tid;
}

void UdpServer::Setup(Ptr<Socket> socket, uint16_t port) {
    m_socket = socket;
    m_port = port;
}

void UdpServer::StartApplication(void) {
    if (!m_socket) {
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        m_socket = Socket::CreateSocket(GetNode(), tid);
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
        m_socket->Bind(local);
    }
    m_socket->SetRecvCallback(MakeCallback(&UdpServer::HandleRead, this));
}

void UdpServer::StopApplication(void) {
    if (m_socket)
        m_socket->Close();
}

void UdpServer::HandleRead(Ptr<Socket> socket) {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from))) {
        NS_LOG_INFO("Received packet of size " << packet->GetSize() << " from " << from);
    }
}

class UdpClient : public Application {
public:
    UdpClient();
    virtual ~UdpClient();

    static TypeId GetTypeId(void);
    void Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, Time interval);

private:
    virtual void StartApplication(void);
    virtual void StopApplication(void);
    void SendPacket(void);

    Ptr<Socket> m_socket;
    Address m_peerAddress;
    uint32_t m_packetSize;
    Time m_interval;
    EventId m_sendEvent;
    bool m_running;
};

UdpClient::UdpClient()
    : m_socket(nullptr), m_peerAddress(), m_packetSize(512), m_interval(Seconds(1)), m_sendEvent(), m_running(false) {}

UdpClient::~UdpClient() {
    m_socket = nullptr;
}

TypeId UdpClient::GetTypeId(void) {
    static TypeId tid = TypeId("UdpClient")
                            .SetParent<Application>()
                            .SetGroupName("Applications")
                            .AddConstructor<UdpClient>();
    return tid;
}

void UdpClient::Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, Time interval) {
    m_socket = socket;
    m_peerAddress = address;
    m_packetSize = packetSize;
    m_interval = interval;
}

void UdpClient::StartApplication(void) {
    m_running = true;
    if (!m_socket) {
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        m_socket = Socket::CreateSocket(GetNode(), tid);
    }

    if (m_socket->Connect(m_peerAddress) == -1) {
        NS_FATAL_ERROR("Failed to connect socket");
    }

    SendPacket();
}

void UdpClient::StopApplication(void) {
    m_running = false;
    if (m_sendEvent.IsRunning())
        Simulator::Cancel(m_sendEvent);
    if (m_socket)
        m_socket->Close();
}

void UdpClient::SendPacket(void) {
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    m_socket->Send(packet);

    NS_LOG_INFO("Sent packet of size " << m_packetSize);

    if (m_running) {
        m_sendEvent = Simulator::Schedule(m_interval, &UdpClient::SendPacket, this);
    }
}

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    mac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;

    // Server setup
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    Ptr<Socket> serverSocket = Socket::CreateSocket(nodes.Get(1), tid);
    auto serverApp = CreateObject<UdpServer>();
    serverApp->Setup(serverSocket, port);
    serverApp->SetStartTime(Seconds(0.0));
    serverApp->SetStopTime(Seconds(10.0));
    nodes.Get(1)->AddApplication(serverApp);

    // Client setup
    Ptr<Socket> clientSocket = Socket::CreateSocket(nodes.Get(0), tid);
    auto clientApp = CreateObject<UdpClient>();
    Address serverAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    clientApp->Setup(clientSocket, serverAddress, 512, Seconds(1));
    clientApp->SetStartTime(Seconds(1.0));
    clientApp->SetStopTime(Seconds(10.0));
    nodes.Get(0)->AddApplication(clientApp);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}