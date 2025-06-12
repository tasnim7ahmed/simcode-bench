#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

class UdpServer : public Application {
public:
    UdpServer() : m_socket(nullptr), m_localPort(9) {}
    virtual ~UdpServer();

    static TypeId GetTypeId(void);
    void Setup(uint16_t port);

private:
    virtual void StartApplication(void);
    virtual void StopApplication(void);
    void HandleRead(Ptr<Socket> socket);

    Ptr<Socket> m_socket;
    uint16_t m_localPort;
};

UdpServer::~UdpServer() {
    if (m_socket) {
        m_socket->Close();
    }
}

TypeId UdpServer::GetTypeId(void) {
    static TypeId tid = TypeId("UdpServer")
                            .SetParent<Application>()
                            .AddConstructor<UdpServer>();
    return tid;
}

void UdpServer::Setup(uint16_t port) {
    m_localPort = port;
}

void UdpServer::StartApplication(void) {
    if (!m_socket) {
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        m_socket = Socket::CreateSocket(GetNode(), tid);
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_localPort);
        m_socket->Bind(local);
        m_socket->SetRecvCallback(MakeCallback(&UdpServer::HandleRead, this));
    }
}

void UdpServer::StopApplication(void) {
    if (m_socket) {
        m_socket->Close();
        m_socket = nullptr;
    }
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
    : m_socket(nullptr), m_packetSize(1024), m_interval(Seconds(1.0)), m_running(false) {}

UdpClient::~UdpClient() {
    m_socket = nullptr;
}

TypeId UdpClient::GetTypeId(void) {
    static TypeId tid = TypeId("UdpClient")
                            .SetParent<Application>()
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
    SendPacket();
}

void UdpClient::StopApplication(void) {
    m_running = false;
    if (m_sendEvent.IsRunning()) {
        Simulator::Cancel(m_sendEvent);
    }
}

void UdpClient::SendPacket(void) {
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    m_socket->SendTo(packet, 0, m_peerAddress);
    NS_LOG_INFO("Sent packet of size " << m_packetSize << " to " << m_peerAddress);
    if (m_running) {
        m_sendEvent = Simulator::Schedule(m_interval, &UdpClient::SendPacket, this);
    }
}

int main(int argc, char *argv[]) {
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    WifiMacHelper wifiMac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

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

    // Server application
    UdpServerHelper serverAppHelper;
    serverAppHelper.SetAttribute("LocalPort", UintegerValue(9));
    ApplicationContainer serverApp = serverAppHelper.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Client application
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    Ptr<Socket> clientSocket = Socket::CreateSocket(nodes.Get(0), tid);
    InetSocketAddress remoteAddr(interfaces.GetAddress(1), 9);
    UdpClientHelper clientAppHelper;
    clientAppHelper.Setup(clientSocket, remoteAddr, 1024, Seconds(1.0));
    ApplicationContainer clientApp = clientAppHelper.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}