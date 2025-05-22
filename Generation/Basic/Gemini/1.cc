#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

// Don't use ns3 namespace (best practice for large projects)
// using namespace ns3; 

int
main(int argc, char* argv[])
{
    // 1. Log setup (optional, but good for debugging)
    // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // 2. Node creation
    NodeContainer nodes;
    nodes.Create(2);

    // 3. Point-to-Point Link Configuration
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // 4. Install Internet Stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // 5. Assign IP Addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // 6. UDP Echo Server Application
    uint16_t port = 9; // Standard Echo Protocol port
    UdpEchoServerHelper echoServer(port);

    ApplicationContainer serverApps = echoServer.Install(nodes.Get(0)); // Server on node 0
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // 7. UDP Echo Client Application
    UdpEchoClientHelper echoClient(interfaces.GetAddress(0), port); // Client targets server's IP (node 0's IP)
    echoClient.SetAttribute("MaxPackets", UintegerValue(1));        // Send a single packet
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));   // Interval between packets (not strictly needed for 1 packet)
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));     // 1024 bytes message

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(1)); // Client on node 1
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0)); // Client stops well after sending its single packet

    // 8. Enable PCAP Capture
    pointToPoint.EnablePcapAll("udp-echo-pcap"); // Capture traffic on all point-to-point devices

    // 9. Set Simulation Stop Time
    Simulator::Stop(Seconds(10.0));

    // 10. Run Simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}