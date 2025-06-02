#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("HybridTopologyExample");

int main (int argc, char *argv[])
{
    // Enable logging for simulation
    LogComponentEnable("HybridTopologyExample", LOG_LEVEL_INFO);

    // Node containers for star topology and bus topology
    NodeContainer starNodes;
    starNodes.Create(4); // Star: 1 central node + 3 peripheral nodes

    NodeContainer busNodes;
    busNodes.Create(3);  // Bus: 3 nodes in bus topology

    // Point-to-Point helper for both star and bus
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Central node connected to 3 peripheral nodes in star topology
    NetDeviceContainer starDevices;
    for (uint32_t i = 1; i < starNodes.GetN(); i++)
    {
        starDevices.Add(p2p.Install(starNodes.Get(0), starNodes.Get(i)));
    }

    // Bus topology connecting 3 nodes in sequence
    NetDeviceContainer busDevices;
    busDevices.Add(p2p.Install(busNodes.Get(0), busNodes.Get(1)));
    busDevices.Add(p2p.Install(busNodes.Get(1), busNodes.Get(2)));

    // Install the Internet stack on all nodes
    InternetStackHelper internet;
    internet.Install(starNodes);
    internet.Install(busNodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ipv4.Assign(starDevices);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    ipv4.Assign(busDevices);

    // Set up UDP echo server on node 0 (central node in star)
    UdpEchoServerHelper echoServer(9); // Port 9
    ApplicationContainer serverApp = echoServer.Install(starNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    // Set up UDP echo clients on nodes 4 and 5 (from bus topology)
    UdpEchoClientHelper echoClient(starNodes.Get(0)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), 9); // Talk to the server
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    clientApps.Add(echoClient.Install(busNodes.Get(0))); // Client on bus node 4
    clientApps.Add(echoClient.Install(busNodes.Get(1))); // Client on bus node 5
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(20.0));

    // Enable routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Enable FlowMonitor to collect statistics
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Run simulation
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    // Print flow monitor statistics
    monitor->CheckForLostPackets();
    monitor->SerializeToXmlFile("hybrid-topology.flowmon", true, true);

    Simulator::Destroy();
    return 0;
}

