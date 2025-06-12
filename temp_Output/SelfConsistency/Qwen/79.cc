#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NetworkTopologySimulation");

int main(int argc, char *argv[])
{
    // Enable logging for this simulation
    LogComponentEnable("NetworkTopologySimulation", LOG_LEVEL_INFO);

    // Declare nodes
    NodeContainer n0, n1, n2, n3;
    n0.Create(1);
    n1.Create(1);
    n2.Create(1);
    n3.Create(1);

    // Create point-to-point links
    PointToPointHelper p2pLowLatency;
    p2pLowLatency.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pLowLatency.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2pHighCost;
    p2pHighCost.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    p2pHighCost.SetChannelAttribute("Delay", StringValue("100ms"));

    PointToPointHelper p2pAltPath;
    p2pAltPath.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    p2pAltPath.SetChannelAttribute("Delay", StringValue("10ms"));

    // Install internet stack
    InternetStackHelper stack;
    stack.InstallAll();

    // Link n0 - n2
    NetDeviceContainer d0d2 = p2pLowLatency.Install(n0.Get(0), n2.Get(0));

    // Link n1 - n2
    NetDeviceContainer d1d2 = p2pLowLatency.Install(n1.Get(0), n2.Get(0));

    // Link n1 - n3 (high cost link)
    NetDeviceContainer d1d3 = p2pHighCost.Install(n1.Get(0), n3.Get(0));

    // Link n2 - n3 (alternate path)
    NetDeviceContainer d2d3 = p2pAltPath.Install(n2.Get(0), n3.Get(0));

    // Assign IP addresses
    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = ipv4.Assign(d0d2);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = ipv4.Assign(d1d2);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i3 = ipv4.Assign(d1d3);

    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i3 = ipv4.Assign(d2d3);

    // Set metric for the high-cost link (n1-n3)
    Ipv4GlobalRoutingHelper routingHelper;
    routingHelper.PopulateRoutingTables();

    // Add a higher metric to the n1-n3 interface to influence routing
    Ptr<Ipv4> ipv4_n1 = n1.Get(0)->GetObject<Ipv4>();
    uint32_t ifIndex_n1_n3 = ipv4_n1->GetInterfaceForAddress(i1i3.GetAddress(0));
    ipv4_n1->SetMetric(ifIndex_n1_n3, 10); // Higher metric

    // Setup UDP traffic from n3 to n1
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(n1.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(i1i3.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(n3.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable tracing of queues and packet receptions
    AsciiTraceHelper asciiTraceHelper;
    p2pHighCost.EnableAsciiAll(asciiTraceHelper.CreateFileStream("topology-trace.tr"));
    p2pHighCost.EnablePcapAll("topology-packet-capture");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}