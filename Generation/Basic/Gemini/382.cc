#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

// NS_LOG_COMPONENT_DEFINE ("CsmaUdpExample"); // No logging as per request, but good practice

int main()
{
    // 1. Create nodes
    ns3::NodeContainer nodes;
    nodes.Create(3);

    // 2. Create CSMA network and set attributes
    ns3::CsmaHelper csma;
    csma.SetDeviceAttribute("DataRate", ns3::StringValue("100Mbps"));
    csma.SetDeviceAttribute("Delay", ns3::StringValue("2ms"));

    ns3::NetDeviceContainer devices = csma.Install(nodes);

    // 3. Install internet stack on all nodes
    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    // 4. Assign IP addresses
    ns3::Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // 5. Setup UDP Server on Node 3 (nodes.Get(2))
    uint16_t port = 9; // Discard port
    ns3::UdpServerHelper serverHelper(port);
    ns3::ApplicationContainer serverApps = serverHelper.Install(nodes.Get(2));
    serverApps.Start(ns3::Seconds(0.0));
    serverApps.Stop(ns3::Seconds(10.0));

    // 6. Setup UDP Client on Node 1 (nodes.Get(0))
    ns3::Ipv4Address serverAddress = interfaces.GetAddress(2); // IP of Node 3
    ns3::UdpClientHelper clientHelper(serverAddress, port);
    clientHelper.SetAttribute("MaxPackets", ns3::UintegerValue(0)); // Send until stop time
    clientHelper.SetAttribute("Interval", ns3::TimeValue(ns3::MilliSeconds(100))); // Send a packet every 100ms
    clientHelper.SetAttribute("PacketSize", ns3::UintegerValue(1024)); // 1024 bytes per packet

    ns3::ApplicationContainer clientApps = clientHelper.Install(nodes.Get(0));
    clientApps.Start(ns3::Seconds(2.0)); // Client starts sending at 2 seconds
    clientApps.Stop(ns3::Seconds(10.0)); // Client stops sending at 10 seconds

    // 7. Set simulation stop time and run
    ns3::Simulator::Stop(ns3::Seconds(10.0));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}