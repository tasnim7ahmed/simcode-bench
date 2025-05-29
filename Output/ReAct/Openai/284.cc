#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure point-to-point channel
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install devices and channels
    NetDeviceContainer devices = p2p.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install TCP sink application on node 1
    uint16_t sinkPort = 9;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), sinkPort));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Install TCP bulk sender on node 0
    BulkSendHelper sourceHelper("ns3::TcpSocketFactory", sinkAddress);
    sourceHelper.SetAttribute("MaxBytes", UintegerValue(0)); // Send unlimited data
    ApplicationContainer sourceApp = sourceHelper.Install(nodes.Get(0));
    sourceApp.Start(Seconds(0.0));
    sourceApp.Stop(Seconds(10.0));

    // Enable PCAP tracing
    p2p.EnablePcapAll("tcp-bulk-send-point-to-point");

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}