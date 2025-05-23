#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

int main(int argc, char *argv[])
{
    // Default simulation parameters
    double simulationTime = 10.0;
    std::string dataRate = "1Mbps";
    uint32_t packetSize = 1024;

    ns3::CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Total duration of the simulation in seconds.", simulationTime);
    cmd.AddValue("dataRate", "On-Off Application data rate (bps).", dataRate);
    cmd.AddValue("packetSize", "On-Off Application packet size (bytes).", packetSize);
    cmd.Parse(argc, argv);

    // Enable logging for relevant components
    ns3::LogComponentEnable("OnOffApplication", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("PacketSink", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("FlowMonitor", ns3::LOG_LEVEL_INFO);

    // Create Nodes
    ns3::Ptr<ns3::Node> r1 = ns3::CreateObject<ns3::Node>(); // Router 1
    ns3::Ptr<ns3::Node> r2 = ns3::CreateObject<ns3::Node>(); // Router 2
    ns3::Ptr<ns3::Node> r3 = ns3::CreateObject<ns3::Node>(); // Router 3
    ns3::Ptr<ns3::Node> lanClient = ns3::CreateObject<ns3::Node>(); // Node in the LAN
    ns3::Ptr<ns3::Node> ptpServer = ns3::CreateObject<ns3::Node>(); // Node connected to R3

    // Install Internet Stack on all nodes
    ns3::InternetStackHelper stack;
    stack.Install(r1);
    stack.Install(r2);
    stack.Install(r3);
    stack.Install(lanClient);
    stack.Install(ptpServer);

    // Configure Point-to-Point Links and assign IP addresses

    // R1-R2 Link
    ns3::PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute("DataRate", ns3::StringValue("100Mbps"));
    p2pHelper.SetChannelAttribute("Delay", ns3::StringValue("2ms"));
    ns3::NetDeviceContainer p2pDevices1;
    p2pDevices1 = p2pHelper.Install(r1, r2);

    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer p2pInterfaces1;
    p2pInterfaces1 = ipv4.Assign(p2pDevices1);

    // R2-R3 Link
    ns3::NetDeviceContainer p2pDevices2;
    p2pDevices2 = p2pHelper.Install(r2, r3);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer p2pInterfaces2;
    p2pInterfaces2 = ipv4.Assign(p2pDevices2);

    // R3-PtpServer Link (where the server resides)
    ns3::NetDeviceContainer p2pDevices3;
    p2pDevices3 = p2pHelper.Install(r3, ptpServer);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer p2pInterfaces3;
    p2pInterfaces3 = ipv4.Assign(p2pDevices3);

    // Configure CSMA LAN Link and assign IP addresses

    // R2-LanClient Link (LAN)
    ns3::CsmaHelper csmaHelper;
    csmaHelper.SetChannelAttribute("DataRate", ns3::StringValue("100Mbps"));
    csmaHelper.SetChannelAttribute("Delay", ns3::StringValue("2us"));

    ns3::NodeContainer lanNodes;
    lanNodes.Add(r2);
    lanNodes.Add(lanClient);
    ns3::NetDeviceContainer csmaDevices;
    csmaDevices = csmaHelper.Install(lanNodes);

    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer csmaInterfaces;
    csmaInterfaces = ipv4.Assign(csmaDevices);

    // Populate routing tables
    ns3::Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup Applications

    // Server (PacketSink) on ptpServer
    uint16_t sinkPort = 9;
    ns3::Address sinkAddress(ns3::InetSocketAddress(p2pInterfaces3.GetAddress(1), sinkPort)); // Address of ptpServer on R3-PtpServer link
    ns3::PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", ns3::InetSocketAddress::GetAny());
    packetSinkHelper.SetAttribute("Local", ns3::AddressValue(sinkAddress));
    ns3::ApplicationContainer serverApps = packetSinkHelper.Install(ptpServer);
    serverApps.Start(ns3::Seconds(0.0));
    serverApps.Stop(ns3::Seconds(simulationTime + 1.0)); // Run a bit longer than client to ensure all packets are received

    // Client (OnOffApplication) on lanClient
    ns3::OnOffHelper onOffHelper("ns3::TcpSocketFactory", ns3::Address());
    onOffHelper.SetAttribute("OnTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute("OffTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=0]")); // Always On
    onOffHelper.SetAttribute("DataRate", ns3::StringValue(dataRate));
    onOffHelper.SetAttribute("PacketSize", ns3::UintegerValue(packetSize));
    onOffHelper.SetAttribute("Remote", ns3::AddressValue(sinkAddress)); // Connect to server's address

    ns3::ApplicationContainer clientApps = onOffHelper.Install(lanClient);
    clientApps.Start(ns3::Seconds(1.0)); // Start client after 1 second
    clientApps.Stop(ns3::Seconds(simulationTime));

    // Enable FlowMonitor
    ns3::FlowMonitorHelper flowMonitorHelper;
    ns3::Ptr<ns3::FlowMonitor> flowMonitor;
    flowMonitor = flowMonitorHelper.InstallAll(); // Install on all nodes
    flowMonitor->Set </* FlowMonitor */ ns3::FlowMonitor>::CheckFor(ns3::FlowMonitorHelper::PROMISCUOUS); // Collect all information

    // Run Simulation
    ns3::Simulator::Stop(ns3::Seconds(simulationTime + 2.0)); // Stop simulation slightly after server
    ns3::Simulator::Run();

    // Process FlowMonitor results and save to XML
    flowMonitor->CheckFor  = ns3::FlowMonitorHelper::PROMISCUOUS; // Ensure promiscuous mode before results collection
    flowMonitor->SerializeToXmlFile("tcp_simulation_results.xml", true, true);

    ns3::Simulator::Destroy();

    return 0;
}