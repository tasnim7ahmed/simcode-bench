#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MultiInterfaceHostStaticRouting");

class PacketLogger {
public:
    void LogRxSource(Ptr<const Packet> packet, const Address& from, const Address& to) {
        std::cout << Simulator::Now().GetSeconds() << "s Source Rx: Packet from " << InetSocketAddress::ConvertFrom(from).GetIpv4()
                  << " to " << InetSocketAddress::ConvertFrom(to).GetIpv4()
                  << ", size " << packet->GetSize() << " bytes" << std::endl;
    }

    void LogRxDestination(Ptr<const Packet> packet, const Address& from, const Address& to) {
        std::cout << Simulator::Now().GetSeconds() << "s Destination Rx: Packet from " << InetSocketAddress::ConvertFrom(from).GetIpv4()
                  << " to " << InetSocketAddress::ConvertFrom(to).GetIpv4()
                  << ", size " << packet->GetSize() << " bytes" << std::endl;
    }
};

class UdpSocketBinder : public Application {
public:
    static TypeId GetTypeId() {
        static TypeId tid = TypeId("UdpSocketBinder")
                            .SetParent<Application>()
                            .AddConstructor<UdpSocketBinder>();
        return tid;
    }

    UdpSocketBinder() {}
    ~UdpSocketBinder() {}

    void Setup(Ptr<Socket> socket, Ipv4Address address, uint16_t port, DataRate dataRate, uint32_t packetSize) {
        m_socket = socket;
        m_peerAddress = address;
        m_peerPort = port;
        m_dataRate = dataRate;
        m_packetSize = packetSize;
    }

protected:
    void StartApplication() override {
        m_sendEvent = Simulator::Schedule(Seconds(0.0), &UdpSocketBinder::SendPacket, this);
    }

    void StopApplication() override {
        if (m_sendEvent.IsRunning()) {
            Simulator::Cancel(m_sendEvent);
        }
    }

    void SendPacket() {
        Ptr<Packet> packet = Create<Packet>(m_packetSize);
        m_socket->Send(packet);

        Time interPacketInterval = Seconds((double)m_packetSize * 8 / m_dataRate.GetBitRate());
        m_sendEvent = Simulator::Schedule(interPacketInterval, &UdpSocketBinder::SendPacket, this);
    }

private:
    Ptr<Socket> m_socket;
    Ipv4Address m_peerAddress;
    uint16_t m_peerPort;
    DataRate m_dataRate;
    uint32_t m_packetSize;
    EventId m_sendEvent;
};

int main(int argc, char* argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Packet::EnablePrinting();

    NodeContainer nodes;
    nodes.Create(4); // source (0), router1 (1), router2 (2), destination (3)

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer link01 = p2p.Install(nodes.Get(0), nodes.Get(1)); // source - router1
    NetDeviceContainer link02 = p2p.Install(nodes.Get(0), nodes.Get(2)); // source - router2
    NetDeviceContainer link13 = p2p.Install(nodes.Get(1), nodes.Get(3)); // router1 - destination
    NetDeviceContainer link23 = p2p.Install(nodes.Get(2), nodes.Get(3)); // router2 - destination

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if01 = ipv4.Assign(link01);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer if02 = ipv4.Assign(link02);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer if13 = ipv4.Assign(link13);

    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer if23 = ipv4.Assign(link23);

    Ipv4StaticRoutingHelper routingHelper;

    // Source node routes
    Ptr<Ipv4StaticRouting> sourceRouting = routingHelper.GetStaticRouting(nodes.Get(0)->GetObject<Ipv4>());
    sourceRouting->AddHostRouteTo(Ipv4Address("10.1.3.2"), Ipv4Address("10.1.1.2"), 1, 10); // metric 10 via router1
    sourceRouting->AddHostRouteTo(Ipv4Address("10.1.4.2"), Ipv4Address("10.1.2.2"), 2, 20); // metric 20 via router2

    // Router1 routes
    Ptr<Ipv4StaticRouting> router1Routing = routingHelper.GetStaticRouting(nodes.Get(1)->GetObject<Ipv4>());
    router1Routing->AddNetworkRouteTo(Ipv4Address("10.1.4.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.1.3.2"), 2, 10); // via router2

    // Router2 routes
    Ptr<Ipv4StaticRouting> router2Routing = routingHelper.GetStaticRouting(nodes.Get(2)->GetObject<Ipv4>());
    router2Routing->AddNetworkRouteTo(Ipv4Address("10.1.3.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.1.4.2"), 2, 10); // via router1

    // Destination route
    Ptr<Ipv4StaticRouting> destRouting = routingHelper.GetStaticRouting(nodes.Get(3)->GetObject<Ipv4>());
    destRouting->AddNetworkRouteTo(Ipv4Address("10.1.1.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.1.3.1"), 1, 10); // via router1
    destRouting->AddNetworkRouteTo(Ipv4Address("10.1.2.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.1.4.1"), 2, 10); // via router2

    uint16_t port = 9;
    PacketLogger logger;

    // Destination application
    UdpServerHelper server(port);
    ApplicationContainer sinkApp = server.Install(nodes.Get(3));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Connect receive callback at destination
    Config::Connect("/NodeList/3/ApplicationList/0/$ns3::UdpServer/Rx", MakeCallback(&PacketLogger::LogRxDestination, &logger));

    // Source application using two sockets on different interfaces
    ApplicationContainer appContainer;

    // First socket bound to interface 1 (10.1.1.1)
    UdpSocketBinderHelper app1Helper;
    app1Helper.SetLocalAddress(if01.GetAddress(0));
    ApplicationContainer app1 = app1Helper.Install(nodes.Get(0));
    app1.Start(Seconds(1.0));
    app1.Stop(Seconds(10.0));
    DynamicCast<UdpSocketBinder>(app1.Get(0))->Setup(
        Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId()),
        if13.GetAddress(1),
        port,
        DataRate("1Mbps"),
        1024);

    // Second socket bound to interface 2 (10.1.2.1)
    UdpSocketBinderHelper app2Helper;
    app2Helper.SetLocalAddress(if02.GetAddress(0));
    ApplicationContainer app2 = app2Helper.Install(nodes.Get(0));
    app2.Start(Seconds(2.0));
    app2.Stop(Seconds(10.0));
    DynamicCast<UdpSocketBinder>(app2.Get(0))->Setup(
        Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId()),
        if23.GetAddress(1),
        port,
        DataRate("1Mbps"),
        512);

    // Connect receive callback at source
    Config::Connect("/NodeList/0/ApplicationList/*/$ns3::UdpSocketBinder/Rx", MakeCallback(&PacketLogger::LogRxSource, &logger));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}