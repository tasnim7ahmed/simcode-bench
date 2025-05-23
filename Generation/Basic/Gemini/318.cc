#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

int main(int argc, char *argv[])
{
    // 1. Create nodes
    ns3::NodeContainer nodes;
    nodes.Create(2); // Node 0 is server, Node 1 is client

    // 2. Create point-to-point link
    ns3::PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute("DataRate", ns3::StringValue("10Mbps"));
    p2pHelper.SetChannelAttribute("Delay", ns3::StringValue("2ms"));
    ns3::NetDeviceContainer devices = p2pHelper.Install(nodes);

    // 3. Install Internet stack
    ns3::InternetStackHelper stackHelper;
    stackHelper.Install(nodes);

    // 4. Assign IP addresses
    ns3::Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Get IP address of the server node (Node 0)
    ns3::Ipv4Address serverIpAddress = interfaces.GetAddress(0);

    // 5. Setup TCP Server (PacketSink)
    uint16_t port = 8080;
    ns3::PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                                     ns3::Address(ns3::InetSocketAddress(serverIpAddress, port)));
    ns3::ApplicationContainer serverApps = sinkHelper.Install(nodes.Get(0)); // Install on Node 0 (server)
    serverApps.Start(ns3::Seconds(1.0)); // Server starts at 1 second
    serverApps.Stop(ns3::Seconds(10.0)); // Server stops at simulation end

    // 6. Setup TCP Client (OnOffApplication)
    ns3::OnOffHelper clientHelper("ns3::TcpSocketFactory",
                                  ns3::Address(ns3::InetSocketAddress(serverIpAddress, port)));
    
    // Client sends 10 TCP packets of 1024 bytes each, with a packet interval of 1 second.
    // For OnOffApplication, a "packet interval of 1 second" for 1024 bytes implies:
    // DataRate = 1024 bytes/sec = 8192 bps.
    // Set OnTime = 1.0s and OffTime = 0.0s, which means it attempts to send for 1s, then immediately tries again.
    // Since each 1024-byte packet at 8192bps takes exactly 1s to transmit, this effectively
    // results in a 1-second interval between the start of consecutive packet transmissions.
    clientHelper.SetAttribute("OnTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    clientHelper.SetAttribute("OffTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    clientHelper.SetAttribute("DataRate", ns3::DataRateValue(ns3::DataRate("8192bps"))); // 1024 bytes/sec = 8192 bps
    clientHelper.SetAttribute("PacketSize", ns3::UintegerValue(1024));
    clientHelper.SetAttribute("nPackets", ns3::UintegerValue(10)); // Send 10 packets

    ns3::ApplicationContainer clientApps = clientHelper.Install(nodes.Get(1)); // Install on Node 1 (client)
    clientApps.Start(ns3::Seconds(2.0)); // Client starts at 2 seconds
    // The client will automatically stop after sending 10 packets due to 'nPackets' attribute.
    // Setting stop time explicitly beyond simulation end to ensure nPackets controls duration.
    clientApps.Stop(ns3::Seconds(12.0)); 

    // 7. Run simulation
    ns3::Simulator::Stop(ns3::Seconds(10.0)); // Simulation runs for 10 seconds
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}