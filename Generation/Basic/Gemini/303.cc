#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

int main()
{
    // LogComponentEnable("PointToPointTcp", ns3::LOG_LEVEL_INFO); // Optional: Enable logging for specific components
    // LogComponentEnable("PacketSink", ns3::LOG_LEVEL_INFO);
    // LogComponentEnable("OnOffApplication", ns3::LOG_LEVEL_INFO);

    // 1. Create two nodes
    ns3::NodeContainer nodes;
    nodes.Create(2); // Creates Node 0 and Node 1

    // 2. Configure Point-to-Point link attributes
    ns3::PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", ns3::StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", ns3::StringValue("2ms"));

    // Install net devices on nodes
    ns3::NetDeviceContainer devices;
    devices = p2p.Install(nodes);

    // 3. Install Internet stack (IP, TCP, UDP, etc.)
    ns3::InternetStackHelper internet;
    internet.Install(nodes);

    // 4. Assign IP addresses to devices
    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0"); // Set the network address and mask
    ns3::Ipv4InterfaceContainer ipv4Interfaces;
    ipv4Interfaces = ipv4.Assign(devices);

    // Get the IP address of Node 1 (server)
    ns3::Ipv4Address serverIpAddress = ipv4Interfaces.GetAddress(1); // Node 1 is at index 1

    // Define the port for TCP communication
    uint16_t port = 8080;

    // 5. Setup TCP Server (Node 1)
    // PacketSinkHelper listens for incoming TCP traffic
    ns3::PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                                     ns3::InetSocketAddress(ns3::Ipv4Address::Any, port));
    ns3::ApplicationContainer serverApps = sinkHelper.Install(nodes.Get(1)); // Install on Node 1
    serverApps.Start(ns3::Seconds(1.0)); // Server starts at 1 second
    serverApps.Stop(ns3::Seconds(10.0)); // Server stops at 10 seconds

    // 6. Setup TCP Client (Node 0)
    // OnOffHelper generates traffic
    ns3::OnOffHelper onOffHelper("ns3::TcpSocketFactory",
                                  ns3::Address(ns3::InetSocketAddress(serverIpAddress, port)));
    // Configure OnOffHelper to always be "On" until MaxBytes are sent or simulation ends
    onOffHelper.SetAttribute("OnTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute("OffTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffHelper.SetAttribute("PacketSize", ns3::UintegerValue(1024)); // Default packet size
    onOffHelper.SetAttribute("DataRate", ns3::DataRateValue(ns3::DataRate("10Mbps"))); // Client attempts to send at this rate
    onOffHelper.SetAttribute("MaxBytes", ns3::UintegerValue(500000)); // Send a maximum of 500,000 bytes

    ns3::ApplicationContainer clientApps = onOffHelper.Install(nodes.Get(0)); // Install on Node 0
    clientApps.Start(ns3::Seconds(2.0)); // Client starts at 2 seconds
    clientApps.Stop(ns3::Seconds(10.0)); // Client stops at 10 seconds

    // 7. Configure simulation time
    ns3::Simulator::Stop(ns3::Seconds(10.0)); // Simulation stops at 10 seconds

    // Run the simulation
    ns3::Simulator::Run();
    ns3::Simulator::Destroy(); // Cleanup

    return 0;
}