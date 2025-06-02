#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FatTreeDataCenter");

int main(int argc, char *argv[])
{
    // Simulation parameters
    uint32_t numServers = 4;
    double simulationTime = 10.0; // seconds

    // Create nodes
    NodeContainer coreSwitch;
    coreSwitch.Create(1); // Single core switch

    NodeContainer aggregationSwitches;
    aggregationSwitches.Create(2); // Two aggregation switches

    NodeContainer edgeSwitches;
    edgeSwitches.Create(2); // Two edge switches

    NodeContainer servers;
    servers.Create(numServers); // Four servers

    // Point-to-Point link attributes
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Connect core to aggregation switches
    NetDeviceContainer coreToAggDevices;
    coreToAggDevices.Add(pointToPoint.Install(coreSwitch.Get(0), aggregationSwitches.Get(0)));
    coreToAggDevices.Add(pointToPoint.Install(coreSwitch.Get(0), aggregationSwitches.Get(1)));

    // Connect aggregation switches to edge switches
    NetDeviceContainer aggToEdgeDevices;
    aggToEdgeDevices.Add(pointToPoint.Install(aggregationSwitches.Get(0), edgeSwitches.Get(0)));
    aggToEdgeDevices.Add(pointToPoint.Install(aggregationSwitches.Get(0), edgeSwitches.Get(1)));
    aggToEdgeDevices.Add(pointToPoint.Install(aggregationSwitches.Get(1), edgeSwitches.Get(0)));
    aggToEdgeDevices.Add(pointToPoint.Install(aggregationSwitches.Get(1), edgeSwitches.Get(1)));

    // Connect edge switches to servers
    NetDeviceContainer edgeToServerDevices;
    for (uint32_t i = 0; i < numServers / 2; ++i)
    {
        edgeToServerDevices.Add(pointToPoint.Install(edgeSwitches.Get(0), servers.Get(i)));
        edgeToServerDevices.Add(pointToPoint.Install(edgeSwitches.Get(1), servers.Get(i + 2)));
    }

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(coreSwitch);
    stack.Install(aggregationSwitches);
    stack.Install(edgeSwitches);
    stack.Install(servers);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces;
    interfaces.Add(ipv4.Assign(coreToAggDevices));
    interfaces.Add(ipv4.Assign(aggToEdgeDevices));
    interfaces.Add(ipv4.Assign(edgeToServerDevices));

    // Install applications (TCP traffic)
    uint16_t port = 9; // TCP port

    // Create server applications
    ApplicationContainer serverApps;
    for (uint32_t i = 0; i < numServers; ++i)
    {
        Address serverAddress(InetSocketAddress(interfaces.GetAddress(i + 6), port));
        PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
        serverApps.Add(packetSinkHelper.Install(servers.Get(i)));
    }
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime));

    // Create client applications to generate TCP traffic
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numServers; ++i)
    {
        OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(i + 6), port));
        onoff.SetAttribute("DataRate", StringValue("1Gbps"));
        onoff.SetAttribute("PacketSize", UintegerValue(1024));
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        clientApps.Add(onoff.Install(servers.Get((i + 1) % numServers)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime));

    // Enable NetAnim visualization
    AnimationInterface anim("fat_tree_datacenter.xml");
    anim.SetConstantPosition(coreSwitch.Get(0), 50.0, 50.0);
    anim.SetConstantPosition(aggregationSwitches.Get(0), 30.0, 30.0);
    anim.SetConstantPosition(aggregationSwitches.Get(1), 70.0, 30.0);
    anim.SetConstantPosition(edgeSwitches.Get(0), 20.0, 10.0);
    anim.SetConstantPosition(edgeSwitches.Get(1), 80.0, 10.0);
    anim.SetConstantPosition(servers.Get(0), 10.0, 0.0);
    anim.SetConstantPosition(servers.Get(1), 30.0, 0.0);
    anim.SetConstantPosition(servers.Get(2), 70.0, 0.0);
    anim.SetConstantPosition(servers.Get(3), 90.0, 0.0);

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

