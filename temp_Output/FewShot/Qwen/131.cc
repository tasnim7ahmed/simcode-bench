#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/dv-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set simulation time
    double stopTime = 50.0;

    // Enable routing logs
    LogComponentEnable("DvRoutingProtocol", LOG_LEVEL_INFO);

    // Create 3 nodes representing routers A, B, C
    NodeContainer nodes;
    nodes.Create(3);

    // Create point-to-point links between A-B and B-C
    PointToPointHelper p2pAB;
    p2pAB.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    p2pAB.SetChannelAttribute("Delay", StringValue("1ms"));

    PointToPointHelper p2pBC;
    p2pBC.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    p2pBC.SetChannelAttribute("Delay", StringValue("1ms"));

    // Install links
    NetDeviceContainer devicesAB = p2pAB.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devicesBC = p2pBC.Install(nodes.Get(1), nodes.Get(2));

    // Install internet stack with DV routing
    DvRoutingHelper dv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(dv); // has to be set before installing
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipAB;
    ipAB.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesAB = ipAB.Assign(devicesAB);

    Ipv4AddressHelper ipBC;
    ipBC.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesBC = ipBC.Assign(devicesBC);

    // Create ping application from node 0 (A) to node 2 (C)
    V4PingHelper ping(interfacesBC.GetAddress(1)); // IP of node C
    ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping.SetAttribute("Verbose", BooleanValue(true));
    ApplicationContainer apps = ping.Install(nodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(stopTime - 1.0));

    // Schedule link failure between B and C at 10 seconds
    Simulator::Schedule(Seconds(10.0), &Ipv4InterfaceContainer::SetDown, &interfacesBC, 1);

    // Stop simulation after defined time
    Simulator::Stop(Seconds(stopTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}