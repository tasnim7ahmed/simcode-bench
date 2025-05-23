#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ospfv2-helper.h"

int main(int argc, char *argv[])
{
    // Enable logging for relevant components to observe OSPF behavior
    LogComponentEnable("Ospfv2Lsa", LOG_LEVEL_INFO);
    LogComponentEnable("Ospfv2RoutingProtocol", LOG_LEVEL_INFO);
    LogComponentEnable("Ospfv2LinkStateDatabase", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Set simulation time
    double simTime = 60.0; // seconds

    // 1. Create nodes (routers)
    NodeContainer routers;
    routers.Create(4);
    Ptr<Node> r1 = routers.Get(0);
    Ptr<Node> r2 = routers.Get(1);
    Ptr<Node> r3 = routers.Get(2);
    Ptr<Node> r4 = routers.Get(3);

    // 2. Install Internet Stack with OSPFv2
    Ospfv2Helper ospf;
    InternetStackHelper internet;
    internet.SetOspfv2Helper(ospf); // Tell InternetStackHelper to use OSPFv2
    internet.Install(routers);

    // 3. Create Point-to-Point links and assign IP addresses
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devicesR1R2, devicesR1R3, devicesR2R4, devicesR3R4;
    Ipv4InterfaceContainer ifacesR1R2, ifacesR1R3, ifacesR2R4, ifacesR3R4;

    Ipv4AddressHelper ipv4;

    // Link R1-R2
    devicesR1R2 = p2p.Install(r1, r2);
    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    ifacesR1R2 = ipv4.Assign(devicesR1R2);

    // Link R1-R3
    devicesR1R3 = p2p.Install(r1, r3);
    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    ifacesR1R3 = ipv4.Assign(devicesR1R3);

    // Link R2-R4
    devicesR2R4 = p2p.Install(r2, r4);
    ipv4.SetBase("10.0.3.0", "255.255.255.0");
    ifacesR2R4 = ipv4.Assign(devicesR2R4);

    // Link R3-R4
    devicesR3R4 = p2p.Install(r3, r4);
    ipv4.SetBase("10.0.4.0", "255.255.255.0");
    ifacesR3R4 = ipv4.Assign(devicesR3R4);

    // 4. Configure OSPF Areas and Interfaces
    // All interfaces are part of the backbone area (Area 0)
    Ptr<Ospfv2Area> area = CreateObject<Ospfv2Area>(Ipv4Address("0.0.0.0")); // Backbone Area
    ospf.AddArea(area);

    // Add router interfaces to the OSPF area
    // R1 interfaces
    ospf.AddInterface(area, ifacesR1R2.Get(0));
    ospf.AddInterface(area, ifacesR1R3.Get(0));
    // R2 interfaces
    ospf.AddInterface(area, ifacesR1R2.Get(1));
    ospf.AddInterface(area, ifacesR2R4.Get(0));
    // R3 interfaces
    ospf.AddInterface(area, ifacesR1R3.Get(1));
    ospf.AddInterface(area, ifacesR3R4.Get(0));
    // R4 interfaces
    ospf.AddInterface(area, ifacesR2R4.Get(1));
    ospf.AddInterface(area, ifacesR3R4.Get(1));

    // 5. Generate application traffic (UDP Echo between R1 and R4)
    // R4 will act as the server, R1 as the client
    uint16_t port = 9; // Discard port

    // Install UdpEchoServer on R4 (using its IP address on the 10.0.4.0 subnet)
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(r4);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime - 1.0));

    // Install UdpEchoClient on R1, targeting R4's IP address on the 10.0.4.0 subnet
    // This traffic will flow through the OSPF routes.
    UdpEchoClientHelper echoClient(ifacesR3R4.GetAddress(1), port); // R4's IP on R3-R4 link
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(r1);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime - 1.0));

    // 6. Simulate network change: Bring down R2-R4 link and bring it back up
    // Get NetDevice objects for the R2-R4 link
    Ptr<PointToPointNetDevice> r2r4_r2_dev = DynamicCast<PointToPointNetDevice>(devicesR2R4.Get(0));
    Ptr<PointToPointNetDevice> r2r4_r4_dev = DynamicCast<PointToPointNetDevice>(devicesR2R4.Get(1));

    // Schedule to bring down the R2-R4 link at 20 seconds
    // This will trigger OSPF to re-calculate routes and generate new LSAs.
    Simulator::Schedule(Seconds(20.0), &PointToPointNetDevice::SetLinkUp, r2r4_r2_dev, false);
    Simulator::Schedule(Seconds(20.0), &PointToPointNetDevice::SetLinkUp, r2r4_r4_dev, false);
    
    // Schedule to bring up the R2-R4 link at 40 seconds
    // This will again trigger OSPF updates.
    Simulator::Schedule(Seconds(40.0), &PointToPointNetDevice::SetLinkUp, r2r4_r2_dev, true);
    Simulator::Schedule(Seconds(40.0), &PointToPointNetDevice::SetLinkUp, r2r4_r4_dev, true);

    // 7. Enable PCAP tracing for all PointToPoint devices
    // This will capture all traffic, including OSPF LSAs and application traffic,
    // allowing visualization in tools like Wireshark.
    p2p.EnablePcapAll("ospf-lsa-simulation");

    // Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}