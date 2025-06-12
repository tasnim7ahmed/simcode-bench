#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    // Logging (optional: enable for routing)
    LogComponentEnable("RipRoutingProtocol", LOG_LEVEL_INFO);

    // Create routers and LAN nodes
    NodeContainer routers;
    routers.Create(3); // R0, R1, R2

    NodeContainer lanNodes1; // Subnet1
    lanNodes1.Create(1);
    NodeContainer lan1;
    lan1.Add(lanNodes1.Get(0));
    lan1.Add(routers.Get(0)); // R0

    NodeContainer lanNodes2; // Subnet2
    lanNodes2.Create(1);
    NodeContainer lan2;
    lan2.Add(lanNodes2.Get(0));
    lan2.Add(routers.Get(0)); // R0

    NodeContainer lanNodes3; // Subnet3
    lanNodes3.Create(1);
    NodeContainer lan3;
    lan3.Add(lanNodes3.Get(0));
    lan3.Add(routers.Get(1)); // R1

    NodeContainer lanNodes4; // Subnet4
    lanNodes4.Create(1);
    NodeContainer lan4;
    lan4.Add(lanNodes4.Get(0));
    lan4.Add(routers.Get(2)); // R2

    // Set up point-to-point links between routers
    // R0 <-> R1
    NodeContainer r0r1;
    r0r1.Add(routers.Get(0));
    r0r1.Add(routers.Get(1));

    // R1 <-> R2
    NodeContainer r1r2;
    r1r2.Add(routers.Get(1));
    r1r2.Add(routers.Get(2));

    // R0 <-> R2
    NodeContainer r0r2;
    r0r2.Add(routers.Get(0));
    r0r2.Add(routers.Get(2));

    // CSMA for LANs
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    // Install CSMA devices on LANs
    NetDeviceContainer lan1Devices = csma.Install(lan1);
    NetDeviceContainer lan2Devices = csma.Install(lan2);
    NetDeviceContainer lan3Devices = csma.Install(lan3);
    NetDeviceContainer lan4Devices = csma.Install(lan4);

    // Point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install p2p on router-router links
    NetDeviceContainer r0r1Devices = p2p.Install(r0r1);
    NetDeviceContainer r1r2Devices = p2p.Install(r1r2);
    NetDeviceContainer r0r2Devices = p2p.Install(r0r2);

    // Install Internet stack with RIP (Distance Vector Routing)
    RipHelper rip;
    InternetStackHelper stack;
    stack.SetRoutingHelper(rip);
    stack.Install(routers);

    // Install basic Internet stack on LAN nodes
    InternetStackHelper lanStack;
    lanStack.Install(lanNodes1);
    lanStack.Install(lanNodes2);
    lanStack.Install(lanNodes3);
    lanStack.Install(lanNodes4);

    // Assign IP addresses
    Ipv4AddressHelper address;

    // LAN 1: 10.1.1.0/24
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer lan1Interfaces = address.Assign(lan1Devices);

    // LAN 2: 10.1.2.0/24
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer lan2Interfaces = address.Assign(lan2Devices);

    // LAN 3: 10.1.3.0/24
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer lan3Interfaces = address.Assign(lan3Devices);

    // LAN 4: 10.1.4.0/24
    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer lan4Interfaces = address.Assign(lan4Devices);

    // R0-R1: 10.1.5.0/30
    address.SetBase("10.1.5.0", "255.255.255.252");
    Ipv4InterfaceContainer r0r1Interfaces = address.Assign(r0r1Devices);

    // R1-R2: 10.1.6.0/30
    address.SetBase("10.1.6.0", "255.255.255.252");
    Ipv4InterfaceContainer r1r2Interfaces = address.Assign(r1r2Devices);

    // R0-R2: 10.1.7.0/30
    address.SetBase("10.1.7.0", "255.255.255.252");
    Ipv4InterfaceContainer r0r2Interfaces = address.Assign(r0r2Devices);

    // Enable global routing on LAN nodes
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Enable RIP interface on routers (link-local and point-to-point)
    // We rely on RipHelper to automatically enable all router interfaces.

    // Set default routes for each LAN node to its router
    Ptr<Ipv4StaticRouting> staticRouting;
    // LAN1 -> R0
    staticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting>(lanNodes1.Get(0)->GetObject<Ipv4>()->GetRoutingProtocol());
    staticRouting->SetDefaultRoute(lan1Interfaces.GetAddress(1), 1);
    // LAN2 -> R0
    staticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting>(lanNodes2.Get(0)->GetObject<Ipv4>()->GetRoutingProtocol());
    staticRouting->SetDefaultRoute(lan2Interfaces.GetAddress(1), 1);
    // LAN3 -> R1
    staticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting>(lanNodes3.Get(0)->GetObject<Ipv4>()->GetRoutingProtocol());
    staticRouting->SetDefaultRoute(lan3Interfaces.GetAddress(1), 1);
    // LAN4 -> R2
    staticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting>(lanNodes4.Get(0)->GetObject<Ipv4>()->GetRoutingProtocol());
    staticRouting->SetDefaultRoute(lan4Interfaces.GetAddress(1), 1);

    // Set up UDP traffic from LAN1 node to LAN4 node
    uint16_t port = 6000;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(lanNodes4.Get(0));
    serverApp.Start(Seconds(2.0));
    serverApp.Stop(Seconds(16.0));

    UdpEchoClientHelper echoClient(lan4Interfaces.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(2.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApp = echoClient.Install(lanNodes1.Get(0));
    clientApp.Start(Seconds(4.0));
    clientApp.Stop(Seconds(16.0));

    // Enable pcap tracing
    csma.EnablePcapAll("dvr-lans", false);
    p2p.EnablePcapAll("dvr-routers", false);

    // Enable pcap on router devices for routing updates
    for (uint32_t i = 0; i < routers.GetN(); ++i)
    {
        Ptr<Node> router = routers.Get(i);
        for (uint32_t j = 0; j < router->GetNDevices(); ++j)
        {
            Ptr<NetDevice> dev = router->GetDevice(j);
            Ptr<PointToPointNetDevice> ptpDev = dev->GetObject<PointToPointNetDevice>();
            if (ptpDev)
            {
                std::ostringstream oss;
                oss << "router-" << i << "-" << j;
                ptpDev->TraceConnect("PhyRxDrop", "", MakeCallback([](Ptr<const Packet>){}) );
                p2p.EnablePcap(oss.str(), dev, true, true);
            }
        }
    }

    Simulator::Stop(Seconds(18.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}