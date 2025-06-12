#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MultiInterfaceHostStaticRouting");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer source;
    source.Create(1);

    NodeContainer routers;
    routers.Create(2); // Rtr1 and Rtr2

    NodeContainer destinationRouter;
    destinationRouter.Create(1);

    NodeContainer destination;
    destination.Create(1);

    // Point-to-point link helper
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install internet stacks
    InternetStackHelper stack;
    stack.InstallAll();

    // Assign IP addresses
    Ipv4AddressHelper address;

    // Link: Source -> Rtr1
    address.SetBase("10.1.1.0", "255.255.255.0");
    NetDeviceContainer sourceRtr1Devices = p2p.Install(source.Get(0), routers.Get(0));
    Ipv4InterfaceContainer sourceRtr1Interfaces = address.Assign(sourceRtr1Devices);

    // Link: Source -> Rtr2
    address.SetBase("10.1.2.0", "255.255.255.0");
    NetDeviceContainer sourceRtr2Devices = p2p.Install(source.Get(0), routers.Get(1));
    Ipv4InterfaceContainer sourceRtr2Interfaces = address.Assign(sourceRtr2Devices);

    // Link: Rtr1 -> Destination Router
    address.SetBase("10.1.3.0", "255.255.255.0");
    NetDeviceContainer rtr1DestRtrDevices = p2p.Install(routers.Get(0), destinationRouter.Get(0));
    Ipv4InterfaceContainer rtr1DestRtrInterfaces = address.Assign(rtr1DestRtrDevices);

    // Link: Rtr2 -> Destination Router
    address.SetBase("10.1.4.0", "255.255.255.0");
    NetDeviceContainer rtr2DestRtrDevices = p2p.Install(routers.Get(1), destinationRouter.Get(0));
    Ipv4InterfaceContainer rtr2DestRtrInterfaces = address.Assign(rtr2DestRtrDevices);

    // Link: Destination Router -> Destination
    address.SetBase("10.1.5.0", "255.255.255.0");
    NetDeviceContainer destRtrDestDevices = p2p.Install(destinationRouter.Get(0), destination.Get(0));
    Ipv4InterfaceContainer destRtrDestInterfaces = address.Assign(destRtrDestDevices);

    // Set up static routing
    Ipv4StaticRoutingHelper routingHelper;

    Ptr<Ipv4StaticRouting> sourceRtr1Routing = routingHelper.GetStaticRouting(source.Get(0)->GetObject<Ipv4>());
    sourceRtr1Routing->AddHostRouteTo(destRtrDestInterfaces.GetAddress(1), sourceRtr1Interfaces.GetAddress(1), 1); // Route via Rtr1

    Ptr<Ipv4StaticRouting> sourceRtr2Routing = routingHelper.GetStaticRouting(source.Get(0)->GetObject<Ipv4>());
    sourceRtr2Routing->AddHostRouteTo(destRtrDestInterfaces.GetAddress(1), sourceRtr2Interfaces.GetAddress(1), 1); // Route via Rtr2

    // Routes for routers
    // Rtr1 to Destination via Destination Router
    Ptr<Ipv4StaticRouting> rtr1Routing = routingHelper.GetStaticRouting(routers.Get(0)->GetObject<Ipv4>());
    rtr1Routing->AddNetworkRouteTo("10.1.5.0", "255.255.255.0", rtr1DestRtrInterfaces.GetAddress(1), 1);

    // Rtr2 to Destination via Destination Router
    Ptr<Ipv4StaticRouting> rtr2Routing = routingHelper.GetStaticRouting(routers.Get(1)->GetObject<Ipv4>());
    rtr2Routing->AddNetworkRouteTo("10.1.5.0", "255.255.255.0", rtr2DestRtrInterfaces.GetAddress(1), 1);

    // Destination router routes to Source through both paths (for bidirectional communication)
    Ptr<Ipv4StaticRouting> destRtrRouting = routingHelper.GetStaticRouting(destinationRouter.Get(0)->GetObject<Ipv4>());
    destRtrRouting->AddNetworkRouteTo("10.1.1.0", "255.255.255.0", rtr1DestRtrInterfaces.GetAddress(0), 1);
    destRtrRouting->AddNetworkRouteTo("10.1.2.0", "255.255.255.0", rtr2DestRtrInterfaces.GetAddress(0), 1);

    // Install TCP sink on the destination node
    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(destRtrDestInterfaces.GetAddress(1), sinkPort));

    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(destination.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Create a custom application to send packets using specific interfaces
    class TcpSender : public Application {
    public:
        static TypeId GetTypeId() {
            static TypeId tid = TypeId("TcpSender")
                .SetParent<Application>()
                .AddConstructor<TcpSender>();
            return tid;
        }

        void Setup(Address address, Address localAddress) {
            m_peer = address;
            m_local = localAddress;
        }

    private:
        virtual void StartApplication() override {
            TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
            m_socket = Socket::CreateSocket(GetNode(), tid);
            m_socket->Bind(m_local);
            m_socket->Connect(m_peer);
            m_socket->Send(Create<Packet>(1024));
        }

        virtual void StopApplication() override {
            if (m_socket) {
                m_socket->Close();
            }
        }

        Address m_peer;
        Address m_local;
        Ptr<Socket> m_socket;
    };

    // Install two sender applications from source with different source interfaces
    TcpSenderHelper app1;
    app1.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    app1.SetAttribute("StopTime", TimeValue(Seconds(9.0)));
    app1.Setup(sinkAddress, InetSocketAddress(sourceRtr1Interfaces.GetAddress(0), 0));
    app1.Install(source.Get(0));

    TcpSenderHelper app2;
    app2.SetAttribute("StartTime", TimeValue(2.0));
    app2.SetAttribute("StopTime", TimeValue(9.0));
    app2.Setup(sinkAddress, InetSocketAddress(sourceRtr2Interfaces.GetAddress(0), 0));
    app2.Install(source.Get(0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}