#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"
#include "ns3/ipv6-routing-table-entry.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer hosts;
    hosts.Create(2); // Host 0 and Host 1

    NodeContainer routers;
    routers.Create(4); // R0, R1, R2, R3

    // Assign names for clarity
    Ptr<Node> h0 = hosts.Get(0);
    Ptr<Node> h1 = hosts.Get(1);
    Ptr<Node> r0 = routers.Get(0);
    Ptr<Node> r1 = routers.Get(1);
    Ptr<Node> r2 = routers.Get(2);
    Ptr<Node> r3 = routers.Get(3);

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Connections:
    // [Host 0]---[R0]---[R1]---[R2]---[R3]---[Host 1]
    NetDeviceContainer d_h0r0 = p2p.Install(h0, r0);      // Host 0 <-> R0
    NetDeviceContainer d_r0r1 = p2p.Install(r0, r1);      // R0 <-> R1
    NetDeviceContainer d_r1r2 = p2p.Install(r1, r2);      // R1 <-> R2
    NetDeviceContainer d_r2r3 = p2p.Install(r2, r3);      // R2 <-> R3
    NetDeviceContainer d_r3h1 = p2p.Install(r3, h1);      // R3 <-> Host 1

    // Install Internet stack (with IPv6)
    InternetStackHelper internetv6;
    internetv6.Install(hosts);
    internetv6.Install(routers);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;
    std::vector<Ipv6InterfaceContainer> ifaces(5);

    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    ifaces[0] = ipv6.Assign(d_h0r0);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    ifaces[1] = ipv6.Assign(d_r0r1);

    ipv6.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64));
    ifaces[2] = ipv6.Assign(d_r1r2);

    ipv6.SetBase(Ipv6Address("2001:4::"), Ipv6Prefix(64));
    ifaces[3] = ipv6.Assign(d_r2r3);

    ipv6.SetBase(Ipv6Address("2001:5::"), Ipv6Prefix(64));
    ifaces[4] = ipv6.Assign(d_r3h1);

    for (uint32_t i = 0; i < ifaces.size(); ++i)
    {
        ifaces[i].SetForwarding(0, true);
        ifaces[i].SetForwarding(1, true);
        ifaces[i].SetDefaultRouteInAllNodes(0);
        ifaces[i].SetDefaultRouteInAllNodes(1);
    }

    // Enable global router and assign routes
    Ipv6StaticRoutingHelper ipv6RoutingHelper;

    // Manually setup static routes to force loose source routing via R0->R2->R3
    // We will use type-0 routing header (loose source routing)
    // Applications: Host 0 sends echo to Host 1, source route via [R0,R2,R3]

    // ICMPv6 Server on Host 1
    uint16_t echoPort = 9;
    Icmpv6EchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(h1);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Build destination and source route addresses (Link-local will not work, use global)
    Address serverAddress = Inet6SocketAddress(ifaces[4].GetAddress(1, 1), echoPort);
    Ipv6Address remote = ifaces[4].GetAddress(1, 1); // Host 1

    // For source routing, set up the loose route via R0->R2->R3
    std::vector<Ipv6Address> srcRoute;
    srcRoute.push_back(ifaces[0].GetAddress(1, 1)); // R0
    srcRoute.push_back(ifaces[2].GetAddress(0, 1)); // R2
    srcRoute.push_back(ifaces[3].GetAddress(1, 1)); // R3

    // ICMPv6 Client on Host 0, with loose source routing
    Ptr<Ipv6RawSocketFactory> factory = h0->GetObject<Ipv6RawSocketFactory>();
    Ptr<Socket> srcSocket = Socket::CreateSocket(h0, TypeId::LookupByName("ns3::Ipv6RawSocketFactory"));

    class Icmpv6LooseEchoClient : public Application
    {
    public:
        Icmpv6LooseEchoClient(Address serverAddr, std::vector<Ipv6Address> srcRoute, uint16_t port)
            : m_serverAddr(serverAddr), m_srcRoute(srcRoute), m_port(port) {}
        void StartApplication() override
        {
            Ptr<Socket> socket = Socket::CreateSocket(GetNode(), TypeId::LookupByName("ns3::Ipv6RawSocketFactory"));
            m_socket = socket;
            Simulator::ScheduleWithContext(GetNode()->GetId(),
                                           Seconds(2.0),
                                           &Icmpv6LooseEchoClient::SendEcho, this);
        }
        void SendEcho()
        {
            // Construct ICMPv6 Echo Request
            Icmpv6Echo icmpv6Echo;
            icmpv6Echo.SetSequenceNumber(1);
            icmpv6Echo.SetIdentifier(1);
            uint8_t payload[32];
            for (uint8_t i = 0; i < 32; ++i) payload[i] = i;
            icmpv6Echo.SetData(payload, 32);

            // Serialize ICMPv6
            Ptr<Packet> pkt = icmpv6Echo.Serialize();

            // Create IPv6Header with Routing (Type 0 Header)
            Ipv6Header ipv6Hdr;
            ipv6Hdr.SetSourceAddress(GetNode()->GetObject<Ipv6>()->GetAddress(1, 1).GetAddress());
            ipv6Hdr.SetDestinationAddress(Inet6SocketAddress::ConvertFrom(m_serverAddr).GetIpv6());
            ipv6Hdr.SetNextHeader(Ipv6Header::IPV6_EXT_ROUTING);

            // Build Routing Header
            Ipv6RoutingHeader routingHeader;
            routingHeader.SetRoutingType(Ipv6RoutingHeader::ROUTING_TYPE_SOURCE);
            for (const auto &addr : m_srcRoute)
                routingHeader.AddAddress(addr);

            ipv6Hdr.SetPayloadLength(pkt->GetSize() + routingHeader.GetSerializedSize());
            pkt->AddHeader(routingHeader);
            pkt->AddHeader(ipv6Hdr);

            m_socket->SendTo(pkt, 0, m_serverAddr);
        }
        void StopApplication() override
        {
            if (m_socket) m_socket->Close();
        }

    private:
        Ptr<Socket> m_socket;
        Address m_serverAddr;
        std::vector<Ipv6Address> m_srcRoute;
        uint16_t m_port;
    };

    Ptr<Application> echoClient = CreateObject<Icmpv6LooseEchoClient>(serverAddress, srcRoute, echoPort);
    h0->AddApplication(echoClient);
    echoClient->SetStartTime(Seconds(2.0));
    echoClient->SetStopTime(Seconds(9.0));

    // Enable pcap tracing
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("loose-routing-ipv6.tr"));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}