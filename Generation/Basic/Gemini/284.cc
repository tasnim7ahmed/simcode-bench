#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

int
main(int argc, char* argv[])
{
    // 1. Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // 2. Configure Point-to-Point link characteristics
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = p2p.Install(nodes);

    // 3. Install Internet Stack on nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // 4. Assign IP Addresses to devices
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // 5. Set up TCP Sink (Receiver) on Node 1
    uint16_t sinkPort = 9; // Port for the TCP sink
    // Bind to any local IP address on the sink node
    Address sinkAddress = InetSocketAddress(Ipv4Address::GetAny(), sinkPort);
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1)); // Install on node 1
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // 6. Set up TCP Bulk Sender on Node 0
    // The remote address is the IP address of the sink node (Node 1)
    Address remoteAddress = InetSocketAddress(interfaces.GetAddress(1), sinkPort);
    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", remoteAddress);
    // Set MaxBytes to 0 for infinite send (or until the application stops)
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer senderApp = bulkSendHelper.Install(nodes.Get(0)); // Install on node 0
    senderApp.Start(Seconds(0.0));
    senderApp.Stop(Seconds(10.0));

    // 7. Enable PCAP tracing for all devices in the point-to-point link
    p2p.EnablePcapAll("simple-p2p-tcp");

    // 8. Set simulation duration
    Simulator::Stop(Seconds(10.0));

    // 9. Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}