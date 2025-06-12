#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/socket-factory.h"
#include "ns3/tcp-socket-factory.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MultiInterfaceStaticRoutingSimulation");

class TcpSender : public Application {
public:
    static TypeId GetTypeId() {
        static TypeId tid = TypeId("TcpSender")
            .SetParent<Application>()
            .AddConstructor<TcpSender>()
            .AddAttribute("DestinationAddress", "The destination address to connect to",
                          Ipv4AddressValue(),
                          MakeIpv4AddressAccessor(&TcpSender::m_destAddress),
                          MakeIpv4AddressChecker())
            .AddAttribute("DestinationPort", "The destination port to connect to",
                          UintegerValue(80),
                          MakeUintegerAccessor(&TcpSender::m_destPort),
                          MakeUintegerChecker<uint16_t>());
        return tid;
    }

    TcpSender() : m_socket(nullptr) {}
    virtual ~TcpSender() {}

private:
    void StartApplication() override {
        TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
        m_socket = Socket::CreateSocket(GetNode(), tid);
        InetSocketAddress dest(m_destAddress, m_destPort);
        m_socket->Connect(dest);
        uint32_t dataSize = 1024;
        uint8_t* data = new uint8_t[dataSize];
        memset(data, 0xA5, dataSize);
        m_socket->Send(data, dataSize, 0);
        delete[] data;
    }

    void StopApplication() override {
        if (m_socket)
            m_socket->Close();
    }

    Ptr<Socket> m_socket;
    Ipv4Address m_destAddress;
    uint16_t m_destPort;
};

int main(int argc, char *argv[]) {
    LogComponentEnable("TcpSender", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer source, rtr1, rtr2, rtr3, dest;
    source.Create(1);
    rtr1.Create(1);
    rtr2.Create(1);
    rtr3.Create(1);
    dest.Create(1);

    // Point-to-point helper
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Links: Source -> Rtr1 and Rtr2
    NetDeviceContainer devSourceRtr1 = p2p.Install(NodeContainer(source, rtr1));
    NetDeviceContainer devSourceRtr2 = p2p.Install(NodeContainer(source, rtr2));

    // Links: Rtr1 -> Rtr3 and Rtr2 -> Rtr3
    NetDeviceContainer devRtr1Rtr3 = p2p.Install(NodeContainer(rtr1, rtr3));
    NetDeviceContainer devRtr2Rtr3 = p2p.Install(NodeContainer(rtr2, rtr3));

    // Link: Rtr3 -> Destination
    NetDeviceContainer devRtr3Dest = p2p.Install(NodeContainer(rtr3, dest));

    InternetStackHelper stack;
    stack.InstallAll();

    // Assign IP addresses
    Ipv4AddressHelper ip;
    ip.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ifSourceRtr1 = ip.Assign(devSourceRtr1);
    ip.NewNetwork();
    Ipv4InterfaceContainer ifSourceRtr2 = ip.Assign(devSourceRtr2);
    ip.NewNetwork();
    Ipv4InterfaceContainer ifRtr1Rtr3 = ip.Assign(devRtr1Rtr3);
    ip.NewNetwork();
    Ipv4InterfaceContainer ifRtr2Rtr3 = ip.Assign(devRtr2Rtr3);
    ip.NewNetwork();
    Ipv4InterfaceContainer ifRtr3Dest = ip.Assign(devRtr3Dest);
    ip.NewNetwork();
    Ipv4InterfaceContainer ifDest = ip.Assign(devRtr3Dest.Get(1)->GetNode()->GetObject<Ipv4>()->GetInterfaceForDevice(devRtr3Dest.Get(1)));
    ip.NewNetwork();

    Ipv4Address destIp = ifRtr3Dest.GetAddress(1); // Destination node's IP

    // Static routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Manually add specific routes from source through either Rtr1 or Rtr2
    Ptr<Ipv4> ipv4Source = source.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4GlobalRouting> routeSource = DynamicCast<Ipv4GlobalRouting>(ipv4Source->GetRoutingProtocol());

    // Route via Rtr1
    routeSource->AddHostRouteTo(destIp, ifSourceRtr1.GetAddress(1), 1);

    // Create sender application on source node
    ApplicationContainer apps;
    TcpSenderHelper senderHelper(destIp, 80);
    apps.Add(senderHelper.Install(source.Get(0)));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}