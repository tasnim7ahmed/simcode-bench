#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/dv-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TwoNodeLoopDvRouting");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes A and B
    NodeContainer nodes;
    nodes.Create(2);

    // Setup point-to-point links between A and B
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devicesAB = p2p.Install(nodes);

    // Install Internet stack with DV routing
    InternetStackHelper stack;
    DvRoutingHelper dv;
    stack.SetRoutingHelper(dv); // has to be set before installing
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesAB = address.Assign(devicesAB);

    // Create a common destination node C connected only to node A initially
    NodeContainer nodeC;
    nodeC.Create(1);

    NetDeviceContainer deviceAC = p2p.Install(nodes.Get(0), nodeC.Get(0));
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaceAC = address.Assign(deviceAC);

    // Allow time for routing tables to converge
    Simulator::Stop(Seconds(4.0));

    // After convergence, disconnect node C from A to simulate failure
    Simulator::Schedule(Seconds(4.0), &Ipv4InterfaceContainer::SetDown, &interfaceAC, 0);

    // Schedule loop behavior: after some time, observe looping
    Simulator::Stop(Seconds(10.0));

    // Setup applications to generate traffic to the now unreachable node C
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodeC);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfacesAB.GetAddress(1), 9); // Send to B (will forward)
    echoClient.SetAttribute("MaxPackets", UintegerValue(20));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0)); // From A
    clientApps.Start(Seconds(5.0));
    clientApps.Stop(Seconds(10.0));

    // Enable ASCII tracing
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("two-node-loop-dv.tr"));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}