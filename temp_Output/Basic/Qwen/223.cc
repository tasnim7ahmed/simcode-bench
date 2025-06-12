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
    UdpServer() : m_socket(nullptr) {}
    virtual ~UdpServer() { delete m_socket; }

    static TypeId GetTypeId() {
        static TypeId tid = TypeId("UdpServer")
            .SetParent<Application>()
            .AddConstructor<UdpServer>();
        return tid;
    }

protected:
    void DoInitialize() override {
        Application::DoInitialize();
        m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
        m_socket->Bind(local);
        m_socket->SetRecvCallback(MakeCallback(&UdpServer::HandleRead, this));
    }

    void DoDispose() override {
        m_socket->Close();
        Application::DoDispose();
    }

private:
    void HandleRead(Ptr<Socket> socket) {
        Ptr<Packet> packet;
        Address from;
        while ((packet = socket->RecvFrom(from))) {
            NS_LOG_INFO("Server received packet of size " << packet->GetSize() << " from " << from);
        }
    }

    Ptr<Socket> m_socket;
};

class UdpClient : public Application {
public:
    UdpClient() : m_socket(nullptr), m_interval(Seconds(1)), m_packetSize(1024) {}
    virtual ~UdpClient() { delete m_socket; }

    static TypeId GetTypeId() {
        static TypeId tid = TypeId("UdpClient")
            .SetParent<Application>()
            .AddConstructor<UdpClient>()
            .AddAttribute("RemoteAddress", "The destination Address of the server",
                          AddressValue(),
                          MakeAddressAccessor(&UdpClient::m_peerAddress),
                          MakeAddressChecker())
            .AddAttribute("Interval", "Time between packets",
                          TimeValue(Seconds(1)),
                          MakeTimeAccessor(&UdpClient::m_interval),
                          MakeTimeChecker())
            .AddAttribute("PacketSize", "Size of each packet",
                          UintegerValue(1024),
                          MakeUintegerAccessor(&UdpClient::m_packetSize),
                          MakeUintegerChecker<uint32_t>());
        return tid;
    }

protected:
    void DoInitialize() override {
        Application::DoInitialize();
        m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        m_sendEvent = Simulator::ScheduleNow(&UdpClient::SendPacket, this);
    }

    void DoDispose() override {
        m_socket->Close();
        Application::DoDispose();
    }

private:
    void SendPacket() {
        Ptr<Packet> packet = Create<Packet>(m_packetSize);
        m_socket->SendTo(packet, 0, m_peerAddress);
        NS_LOG_INFO("Client sent packet of size " << m_packetSize << " to " << m_peerAddress);
        m_sendEvent = Simulator::Schedule(m_interval, &UdpClient::SendPacket, this);
    }

    Ptr<Socket> m_socket;
    Address m_peerAddress;
    Time m_interval;
    uint32_t m_packetSize;
    EventId m_sendEvent;
};

int main(int argc, char *argv[]) {
    LogComponentEnable("UdpAdHocSimulation", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    WifiMacHelper wifiMac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate6Mbps"));

    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(nodes);

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

    // Install Server
    UdpServerHelper serverApp;
    ApplicationContainer serverContainer = serverApp.Install(nodes.Get(1));
    serverContainer.Start(Seconds(0.0));
    serverContainer.Stop(Seconds(10.0));

    // Install Client
    UdpClientHelper clientApp;
    clientApp.SetAttribute("RemoteAddress", AddressValue(InetSocketAddress(interfaces.GetAddress(1), 9)));
    clientApp.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    clientApp.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientContainer = clientApp.Install(nodes.Get(0));
    clientContainer.Start(Seconds(1.0));
    clientContainer.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}