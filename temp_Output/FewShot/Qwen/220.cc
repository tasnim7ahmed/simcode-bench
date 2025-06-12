#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/application.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"

using namespace ns3;

class UdpServer : public Application {
public:
    UdpServer() : m_socket(nullptr) {}
    virtual ~UdpServer() { }

    static TypeId GetTypeId() {
        static TypeId tid = TypeId("UdpServer")
            .SetParent<Application>()
            .AddConstructor<UdpServer>();
        return tid;
    }

protected:
    void DoInitialize() override {
        Application::DoInitialize();
        if (m_socket == nullptr) {
            TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
            m_socket = Socket::CreateSocket(GetNode(), tid);
            InetSocketAddress local = InetSocketAddress(m_localAddress, m_port);
            m_socket->Bind(local);
            m_socket->SetRecvCallback(MakeCallback(&UdpServer::HandleRead, this));
        }
    }

private:
    void HandleRead(Ptr<Socket> socket) {
        Ptr<Packet> packet;
        Address from;
        while ((packet = socket->RecvFrom(from))) {
            std::cout << Simulator::Now().GetSeconds() << "s Server received packet size: " << packet->GetSize() << " from " << InetSocketAddress::ConvertFrom(from).GetIpv4() << std::endl;
        }
    }

    void StartApplication() override {}
    void StopApplication() override {}

    Ipv4Address m_localAddress = Ipv4Address::GetAny();
    uint16_t m_port = 9;
    Ptr<Socket> m_socket;
};

class UdpClient : public Application {
public:
    UdpClient() {}
    virtual ~UdpClient() { }

    static TypeId GetTypeId() {
        static TypeId tid = TypeId("UdpClient")
            .SetParent<Application>()
            .AddConstructor<UdpClient>()
            .AddAttribute("RemoteAddress", "The destination IP address of the UDP packets.",
                          Ipv4AddressValue(),
                          MakeIpv4AddressAccessor(&UdpClient::m_remoteAddress),
                          MakeIpv4AddressChecker())
            .AddAttribute("RemotePort", "The destination port of the UDP packets.",
                          UintegerValue(9),
                          MakeUintegerAccessor(&UdpClient::m_remotePort),
                          MakeUintegerChecker<uint16_t>())
            .AddAttribute("Interval", "Time between sending each packet.",
                          TimeValue(Seconds(1.0)),
                          MakeTimeAccessor(&UdpClient::m_interval),
                          MakeTimeChecker())
            .AddAttribute("PacketSize", "Size of the packet to send in bytes.",
                          UintegerValue(1024),
                          MakeUintegerAccessor(&UdpClient::m_packetSize),
                          MakeUintegerChecker<uint32_t>());
        return tid;
    }

protected:
    void DoInitialize() override {
        Application::DoInitialize();
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        m_socket = Socket::CreateSocket(GetNode(), tid);
    }

private:
    void StartApplication() override {
        m_sendEvent = Simulator::Schedule(Seconds(0.0), &UdpClient::SendPacket, this);
    }

    void StopApplication() override {
        if (m_sendEvent.IsRunning()) {
            Simulator::Cancel(m_sendEvent);
        }
    }

    void SendPacket() {
        Ptr<Packet> packet = Create<Packet>(m_packetSize);
        m_socket->SendTo(packet, 0, InetSocketAddress(m_remoteAddress, m_remotePort));
        std::cout << Simulator::Now().GetSeconds() << "s Client sent packet size: " << m_packetSize << " to " << m_remoteAddress << ":" << m_remotePort << std::endl;
        m_sendEvent = Simulator::Schedule(m_interval, &UdpClient::SendPacket, this);
    }

    Ipv4Address m_remoteAddress;
    uint16_t m_remotePort;
    Time m_interval;
    uint32_t m_packetSize;
    Ptr<Socket> m_socket;
    EventId m_sendEvent;
};

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install server application
    UdpServerHelper server;
    server.SetAttribute("LocalAddress", Ipv4AddressValue(interfaces.GetAddress(1)));
    server.SetAttribute("Port", UintegerValue(9));
    ApplicationContainer serverApp = server.Install(nodes.Get(1));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    // Install client application
    UdpClientHelper client;
    client.SetAttribute("RemoteAddress", Ipv4AddressValue(interfaces.GetAddress(1)));
    client.SetAttribute("RemotePort", UintegerValue(9));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(0.0));
    clientApp.Stop(Seconds(10.0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}