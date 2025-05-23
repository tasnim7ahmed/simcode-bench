#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

int main()
{
    // 1. Set default TCP congestion control to NewReno
    ns3::Config::SetDefault("ns3::TcpL4Protocol::SocketType", ns3::StringValue("ns3::TcpNewReno"));

    // 2. Create nodes
    ns3::NodeContainer nodes;
    nodes.Create(2); // Node 0 and Node 1

    // 3. Configure and install Point-to-Point link
    ns3::PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", ns3::StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", ns3::StringValue("2ms"));
    ns3::NetDeviceContainer devices = pointToPoint.Install(nodes);

    // 4. Install Internet stack
    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    // 5. Assign IP addresses
    ns3::Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // 6. Setup TCP Server (PacketSink) on Node 1
    uint16_t sinkPort = 9;
    ns3::Address sinkAddress(ns3::InetSocketAddress(interfaces.GetAddress(1), sinkPort)); // Node 1 (index 1)
    ns3::PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), sinkPort));
    ns3::ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
    sinkApp.Start(ns3::Seconds(0.0));
    sinkApp.Stop(ns3::Seconds(10.0));

    // 7. Setup TCP Client (OnOffApplication) on Node 0
    ns3::OnOffHelper onoffHelper("ns3::TcpSocketFactory", sinkAddress);
    onoffHelper.SetAttribute("OnTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    onoffHelper.SetAttribute("OffTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=0.0]")); // Continuous traffic
    onoffHelper.SetAttribute("DataRate", ns3::StringValue("5Mbps")); // Client tries to send at link rate
    onoffHelper.SetAttribute("PacketSize", ns3::UintegerValue(1024)); // 1KB packets

    ns3::ApplicationContainer clientApp = onoffHelper.Install(nodes.Get(0)); // Node 0 (index 0)
    clientApp.Start(ns3::Seconds(1.0)); // Start client after 1 second
    clientApp.Stop(ns3::Seconds(10.0)); // Stop client at 10 seconds

    // 8. Configure FlowMonitor
    ns3::FlowMonitorHelper flowmonHelper;
    ns3::Ptr<ns3::FlowMonitor> flowMonitor = flowmonHelper.InstallAll();

    // 9. Run simulation for 10 seconds
    ns3::Simulator::Stop(ns3::Seconds(10.0));
    ns3::Simulator::Run();

    // 10. Collect and export FlowMonitor statistics to XML
    flowMonitor->CheckFor )\"\"\"(); // Check for any changes in flows
    flowmonHelper.SerializeToXmlFile("FlowMonitor.xml", true, true);

    // 11. Cleanup
    ns3::Simulator::Destroy();

    return 0;
}