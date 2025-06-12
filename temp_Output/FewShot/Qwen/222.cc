#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/socket-factory.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/packet-sink-helper.h"

using namespace ns3;

class UdpServer : public Application {
public:
    static TypeId GetTypeId() {
        static TypeId tid = TypeId("UdpServer")
            .SetParent<Application>()
            .AddConstructor<UdpServer>();
        return tid;
    }

    UdpServer() : m_socket(nullptr) {}
    virtual ~UdpServer() {}

private:
    void StartApplication() override {
        if (!m_socket) {
            TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
            m_socket = Socket::CreateSocket(GetNode(), tid);
            InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
            m_socket->Bind(local);
            m_socket->SetRecvCallback(MakeCallback(&UdpServer::HandleRead, this));
        }
    }

    void StopApplication() override {
        if (m_socket) {
            m_socket->Close();
            m_socket = nullptr;
        }
    }

    void HandleRead(Ptr<Socket> socket) {
        Ptr<Packet> packet;
        Address from;
        while ((packet = socket->RecvFrom(from))) {
            std::cout << "Server received packet of size " << packet->GetSize()
                      << " at time " << Simulator::Now().GetSeconds() << "s" << std::endl;
        }
    }

    Ptr<Socket> m_socket;
};

class UdpClient : public Application {
public:
    static TypeId GetTypeId() {
        static TypeId tid = TypeId("UdpClient")
            .SetParent<Application>()
            .AddConstructor<UdpClient>()
            .AddAttribute("RemoteAddress", "The destination IP address",
                          Ipv4AddressValue(),
                          MakeIpv4AddressAccessor(&UdpClient::m_remoteAddress),
                          MakeIpv4AddressChecker())
            .AddAttribute("RemotePort", "The destination port",
                          UintegerValue(9),
                          MakeUintegerAccessor(&UdpClient::m_remotePort),
                          MakeUintegerChecker<uint16_t>())
            .AddAttribute("Interval", "The interval between packets",
                          TimeValue(Seconds(1.0)),
                          MakeTimeAccessor(&UdpClient::m_interval),
                          MakeTimeChecker())
            .AddAttribute("PacketSize", "The size of each packet",
                          UintegerValue(1024),
                          MakeUintegerAccessor(&UdpClient::m_packetSize),
                          MakeUintegerChecker<uint32_t>());
        return tid;
    }

    UdpClient() : m_socket(nullptr), m_sendEvent(), m_packetsSent(0) {}
    virtual ~UdpClient() {}

private:
    void StartApplication() override {
        if (!m_socket) {
            TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
            m_socket = Socket::CreateSocket(GetNode(), tid);
        }

        ScheduleTransmit(Seconds(0.0));
    }

    void StopApplication() override {
        if (m_sendEvent.IsRunning()) {
            Simulator::Cancel(m_sendEvent);
        }

        if (m_socket) {
            m_socket->Close();
            m_socket = nullptr;
        }
    }

    void ScheduleTransmit(Time dt) {
        m_sendEvent = Simulator::Schedule(dt, &UdpClient::SendPacket, this);
    }

    void SendPacket() {
        Ptr<Packet> packet = Create<Packet>(m_packetSize);
        m_socket->SendTo(packet, 0, InetSocketAddress(m_remoteAddress, m_remotePort));

        std::cout << "Client sent packet of size " << m_packetSize
                  << " to " << m_remoteAddress << ":" << m_remotePort
                  << " at time " << Simulator::Now().GetSeconds() << "s" << std::endl;

        ScheduleTransmit(m_interval);
    }

    Ptr<Socket> m_socket;
    Ipv4Address m_remoteAddress;
    uint16_t m_remotePort;
    Time m_interval;
    uint32_t m_packetSize;
    EventId m_sendEvent;
    uint32_t m_packetsSent;
};

int main(int argc, char *argv[]) {
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(1);

    NodeContainer apNode;
    apNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);

    Ssid ssid = Ssid("ns-3-ssid");

    // Setup AP
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(Seconds(2.5)));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

    // Setup STA
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(apNode);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(apNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);

    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);

    // Applications
    UdpServerHelper server;
    ApplicationContainer serverApp = server.Install(apNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpClientHelper client;
    client.SetAttribute("RemoteAddress", Ipv4AddressValue(apInterface.GetAddress(0)));
    client.SetAttribute("RemotePort", UintegerValue(9));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = client.Install(wifiStaNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}