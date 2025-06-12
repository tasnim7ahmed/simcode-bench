#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/dv-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-static-routing.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CountToInfinitySimulation");

int main(int argc, char *argv[]) {
    bool verbose = true;
    uint32_t stopTime = 30;

    if (verbose) {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    // Create nodes for routers A, B, and C
    NodeContainer nodes;
    nodes.Create(3);

    // Create point-to-point links between A <-> B, B <-> C, and C <-> A (triangle)
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Links: A-B, B-C, C-A
    NetDeviceContainer devAB = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devBC = p2p.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer devCA = p2p.Install(nodes.Get(2), nodes.Get(0));

    // Install Internet stack with Distance Vector routing
    InternetStackHelper stack;
    DvRoutingHelper dv;
    stack.SetRoutingHelper(dv); // has to be set before installing
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ifAB = address.Assign(devAB);
    address.NewNetwork();
    Ipv4InterfaceContainer ifBC = address.Assign(devBC);
    address.NewNetwork();
    Ipv4InterfaceContainer ifCA = address.Assign(devCA);

    // Set routing tables manually if needed? Or wait for DV convergence

    // Create a ping from node A to node C to observe the route
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(2)); // C as server
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(stopTime));

    UdpEchoClientHelper echoClient(ifCA.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(30));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0)); // A as client
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(stopTime));

    // Schedule link failure between B and C at time 5s
    Simulator::Schedule(Seconds(5.0), &Ipv4::SetDown, nodes.Get(1)->GetObject<Ipv4>(), ifBC.GetInterfaceIndex(0));
    Simulator::Schedule(Seconds(5.0), &Ipv4::SetDown, nodes.Get(2)->GetObject<Ipv4>(), ifBC.GetInterfaceIndex(1));

    // Enable ASCII tracing
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("count-to-infinity.tr"));

    // Enable PCAP tracing
    p2p.EnablePcapAll("count-to-infinity");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}