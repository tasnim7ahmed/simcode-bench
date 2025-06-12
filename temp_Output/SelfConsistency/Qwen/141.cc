#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RingTopologySimulation");

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 3 nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Create point-to-point links between adjacent nodes to form a ring
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[3];
    devices[0] = p2p.Install(nodes.Get(0), nodes.Get(1)); // Link node 0 <-> 1
    devices[1] = p2p.Install(nodes.Get(1), nodes.Get(2)); // Link node 1 <-> 2
    devices[2] = p2p.Install(nodes.Get(2), nodes.Get(0)); // Link node 2 <-> 0

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[3];

    address.SetBase("10.1.1.0", "255.255.255.0");
    interfaces[0] = address.Assign(devices[0]);

    address.SetBase("10.1.2.0", "255.255.255.0");
    interfaces[1] = address.Assign(devices[1]);

    address.SetBase("10.1.3.0", "255.255.255.0");
    interfaces[2] = address.Assign(devices[2]);

    // Set up echo server on node 0
    UdpEchoServerHelper echoServer(9); // port 9
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Set up echo client on node 1 sending to node 0
    UdpEchoClientHelper echoClient(interfaces[0].GetAddress(0), 9); // node 0's IP
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable packet logging
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("ring-topology-simulation.tr");
    p2p.EnableAsciiAll(stream);

    // Enable animation (optional)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    AnimationInterface anim("ring-topology.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}