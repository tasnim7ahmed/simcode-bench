#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/ipv6-l3-protocol.h"
#include "ns3/ipv6-header.h"
#include "ns3/packet-sink.h"
#include "ns3/icmpv6-echo.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("Ipv6L3Protocol", LOG_LEVEL_INFO);

    // Create nodes: Host0, Host1, Router0, Router1, Router2, Router3
    NodeContainer hosts, routers, allNodes;
    hosts.Create(2); // host0 (0), host1 (1)
    routers.Create(4); // router0 (2), router1 (3), router2 (4), router3 (5)
    allNodes.Add(hosts);
    allNodes.Add(routers);

    // Create point-to-point links: Host0<->Router0, Router0<->Router1, Router1<->Router2, Router2<->Router3, Router3<->Host1
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d0 = p2p.Install(hosts.Get(0), routers.Get(0));    // Host0 - Router0
    NetDeviceContainer d1 = p2p.Install(routers.Get(0), routers.Get(1));  // Router0 - Router1
    NetDeviceContainer d2 = p2p.Install(routers.Get(1), routers.Get(2));  // Router1 - Router2
    NetDeviceContainer d3 = p2p.Install(routers.Get(2), routers.Get(3));  // Router2 - Router3
    NetDeviceContainer d4 = p2p.Install(routers.Get(3), hosts.Get(1));    // Router3 - Host1

    // Install IPv6 stack
    InternetStackHelper internetv6;
    Ipv6StaticRoutingHelper ipv6RoutingHelper;
    internetv6.SetIpv4StackInstall(false);
    internetv6.Install(allNodes);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;
    std::vector<Ipv6InterfaceContainer> interfaces;

    // Host0<->Router0
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    interfaces.push_back(ipv6.Assign(d0));
    interfaces.back().SetForwarding(1, true);
    interfaces.back().SetDefaultRouteInAllNodes(0);

    // Router0<->Router1
    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    interfaces.push_back(ipv6.Assign(d1));
    interfaces.back().SetForwarding(0,true);
    interfaces.back().SetForwarding(1,true);

    // Router1<->Router2
    ipv6.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64));
    interfaces.push_back(ipv6.Assign(d2));
    interfaces.back().SetForwarding(0,true);
    interfaces.back().SetForwarding(1,true);

    // Router2<->Router3
    ipv6.SetBase(Ipv6Address("2001:4::"), Ipv6Prefix(64));
    interfaces.push_back(ipv6.Assign(d3));
    interfaces.back().SetForwarding(0,true);
    interfaces.back().SetForwarding(1,true);

    // Router3<->Host1
    ipv6.SetBase(Ipv6Address("2001:5::"), Ipv6Prefix(64));
    interfaces.push_back(ipv6.Assign(d4));
    interfaces.back().SetForwarding(0,true);
    interfaces.back().SetDefaultRouteInAllNodes(1);

    // Set up static routing to enable loose source routing
    Ipv6StaticRoutingHelper staticRoutingHelper;

    // Host0: Default route points to Router0
    Ptr<Ipv6StaticRouting> host0Static = staticRoutingHelper.GetStaticRouting(hosts.Get(0)->GetObject<Ipv6>());

    // Host1: Default route points to Router3
    Ptr<Ipv6StaticRouting> host1Static = staticRoutingHelper.GetStaticRouting(hosts.Get(1)->GetObject<Ipv6>());

    // Routers: Enable forwarding, add static routes as necessary
    for (uint32_t i = 0; i < routers.GetN(); ++i)
    {
        routers.Get(i)->GetObject<Ipv6>()->SetAttribute("IpForward", BooleanValue(true));
    }

    // Now configure the ICMPv6 Echo Application with Source Routing option
    // Host0 sends to Host1 using loose source route:
    // waypoints: Router1's d2.Get(0), Router2's d3.Get(0)
    Ipv6Address dstHost1 = interfaces[4].GetAddress(1, 1); // Host1 address on last link
    Ipv6Address waypoint1 = interfaces[2].GetAddress(0, 1); // Router1 address on Router1-Router2 link
    Ipv6Address waypoint2 = interfaces[3].GetAddress(0, 1); // Router2 address on Router2-Router3 link

    // Create a custom source-routing option
    class LooseSourceRoutingOption : public OptionHeader
    {
    public:
        LooseSourceRoutingOption(const std::vector<Ipv6Address>& waypoints)
        : OptionHeader(), m_waypoints(waypoints)
        {
            SetType(0x2B); // Routing Header, type 0 for source routing (deprecated, but for simulation purpose)
        }
        virtual ~LooseSourceRoutingOption() {}

        void Serialize(Buffer::Iterator start) const override
        {
            start.WriteU8(GetType());
            uint8_t segmentsLeft = m_waypoints.size();
            start.WriteU8(segmentsLeft);
            for (const auto& addr : m_waypoints)
            {
                uint8_t buf[16];
                addr.Serialize(buf);
                for (int j = 0; j < 16; j++) start.WriteU8(buf[j]);
            }
        }
        uint32_t GetSerializedSize() const override
        {
            return 2 + m_waypoints.size()*16;
        }
        uint8_t GetType() const override { return 43; }
    private:
        std::vector<Ipv6Address> m_waypoints;
    };

    // Create a custom application
    class CustomIcmpv6EchoApp : public Application
    {
    public:
        CustomIcmpv6EchoApp() {}
        void Setup(Address dst, std::vector<Ipv6Address> waypoints, uint16_t port, uint32_t count, Time interval)
        {
            m_dst = dst;
            m_waypoints = waypoints;
            m_port = port;
            m_count = count;
            m_interval = interval;
        }
    private:
        virtual void StartApplication()
        {
            m_socket = Socket::CreateSocket(GetNode(), TypeId::LookupByName("ns3::Ipv6RawSocketFactory"));
            m_socket->Bind();
            m_sent = 0;
            Send();
        }
        void Send()
        {
            if (m_sent >= m_count) return;

            Ptr<Packet> p = Create<Packet>(100);

            Icmpv6Echo echo;
            echo.SetSequenceNumber(m_sent);
            echo.SetIdentifier(1);
            echo.SetType(Icmpv6Header::ICMPV6_ECHO_REQUEST);

            Icmpv6Header icmp;
            icmp.SetType(Icmpv6Header::ICMPV6_ECHO_REQUEST);
            icmp.SetCode(0);
            icmp.SetChecksum(0);
            icmp.SetData(p);

            p->AddHeader(echo);
            p->AddHeader(icmp);

            Ipv6Header ipv6;
            ipv6.SetSourceAddress(GetNode()->GetObject<Ipv6>()->GetAddress(1,1).GetAddress());
            ipv6.SetDestinationAddress(Inet6SocketAddress::ConvertFrom(m_dst).GetIpv6());
            ipv6.SetNextHeader(43); // Routing Extension Header
            ipv6.SetPayloadLength(p->GetSize());
            ipv6.SetHopLimit(64);

            // Add the "Routing Header" (for loose source routing)
            LooseSourceRoutingOption lsrh(m_waypoints);
            p->AddHeader(lsrh);

            p->AddHeader(ipv6);

            m_socket->SendTo(p, 0, m_dst);

            ++m_sent;
            Simulator::Schedule(m_interval, &CustomIcmpv6EchoApp::Send, this);
        }
        void StopApplication() override
        {
            if (m_socket) m_socket->Close();
        }
        Ptr<Socket> m_socket;
        Address m_dst;
        std::vector<Ipv6Address> m_waypoints;
        uint16_t m_port;
        uint32_t m_count;
        uint32_t m_sent;
        Time m_interval;
    };

    // Install the custom echo app on Host0
    Ptr<CustomIcmpv6EchoApp> echoApp = CreateObject<CustomIcmpv6EchoApp>();
    Address dstAddress = Inet6SocketAddress(dstHost1, 0);
    std::vector<Ipv6Address> waypoints{ waypoint1, waypoint2 };
    echoApp->Setup(dstAddress, waypoints, 0, 4, Seconds(1.0));
    hosts.Get(0)->AddApplication(echoApp);
    echoApp->SetStartTime(Seconds(2.0));
    echoApp->SetStopTime(Seconds(10.0));

    // ICMPv6 Echo Server (use standard server app) on Host1
    Icmpv6EchoServerHelper server(0);
    ApplicationContainer apps = server.Install(hosts.Get(1));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(12.0));

    // Enable packet tracing for all devices
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("loose-routing-ipv6.tr"));

    Simulator::Stop(Seconds(12.1));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}