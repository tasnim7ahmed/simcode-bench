#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/error-model.h"
#include "ns3/drop-tail-queue.h"

int main(int argc, char *argv[])
{
    // Set default values for simulation parameters
    double simTime = 5.0; // seconds

    // Create nodes
    ns3::NodeContainer nodes;
    nodes.Create(4);
    ns3::Ptr<ns3::Node> n0 = nodes.Get(0);
    ns3::Ptr<ns3::Node> n1 = nodes.Get(1);
    ns3::Ptr<ns3::Node> n2 = nodes.Get(2);
    ns3::Ptr<ns3::Node> n3 = nodes.Get(3);

    // Create PointToPointHelper instances for different link configurations
    ns3::PointToPointHelper p2pHelper5Mbps;
    p2pHelper5Mbps.SetDeviceAttribute("DataRate", ns3::StringValue("5Mbps"));
    p2pHelper5Mbps.SetChannelAttribute("Delay", ns3::StringValue("2ms"));
    p2pHelper5Mbps.SetQueue("ns3::DropTailQueue");

    ns3::PointToPointHelper p2pHelper1_5Mbps;
    p2pHelper1_5Mbps.SetDeviceAttribute("DataRate", ns3::StringValue("1.5Mbps"));
    p2pHelper1_5Mbps.SetChannelAttribute("Delay", ns3::StringValue("10ms"));
    p2pHelper1_5Mbps.SetQueue("ns3::DropTailQueue");

    // Install Internet stack on all nodes
    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    // Create links between nodes
    ns3::NetDeviceContainer devices_n0n2 = p2pHelper5Mbps.Install(n0, n2);
    ns3::NetDeviceContainer devices_n1n2 = p2pHelper5Mbps.Install(n1, n2);
    ns3::NetDeviceContainer devices_n3n2 = p2pHelper1_5Mbps.Install(n3, n2);

    // Assign IP addresses to interfaces
    ns3::Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces_n0n2 = address.Assign(devices_n0n2);

    address.SetBase("10.1.2.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces_n1n2 = address.Assign(devices_n1n2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces_n3n2 = address.Assign(devices_n3n2);

    // Create and combine error models
    ns3::Ptr<ns3::RateErrorModel> rateError = ns3::CreateObject<ns3::RateErrorModel>();
    rateError->SetAttribute("ErrorRate", ns3::DoubleValue(0.001));

    ns3::Ptr<ns3::BurstErrorModel> burstError = ns3::CreateObject<ns3::BurstErrorModel>();
    burstError->SetAttribute("ErrorRate", ns3::DoubleValue(0.01));
    // Default BurstSize and Gap are used if not specified, but setting common defaults for clarity:
    burstError->SetAttribute("BurstSize", ns3::StringValue("ns3::ConstantRandomVariable[Constant=3]"));
    burstError->SetAttribute("Gap", ns3::StringValue("ns3::ConstantRandomVariable[Constant=20us]"));

    ns3::Ptr<ns3::ListErrorModel> listError = ns3::CreateObject<ns3::ListErrorModel>();
    listError->Add(11); // Drop packet with UID 11
    listError->Add(17); // Drop packet with UID 17

    ns3::Ptr<ns3::CompositeErrorModel> compositeError = ns3::CreateObject<ns3::CompositeErrorModel>();
    compositeError->Add(rateError);
    compositeError->Add(burstError);
    compositeError->Add(listError);

    // Apply the composite error model to the receive side of all point-to-point devices
    // This makes errors occur on incoming packets on all P2P links.
    for (uint32_t i = 0; i < devices_n0n2.GetN(); ++i)
    {
        devices_n0n2.Get(i)->SetAttribute("ReceiveErrorModel", ns3::PointerValue(compositeError));
    }
    for (uint32_t i = 0; i < devices_n1n2.GetN(); ++i)
    {
        devices_n1n2.Get(i)->SetAttribute("ReceiveErrorModel", ns3::PointerValue(compositeError));
    }
    for (uint32_t i = 0; i < devices_n3n2.GetN(); ++i)
    {
        devices_n3n2.Get(i)->SetAttribute("ReceiveErrorModel", ns3::PointerValue(compositeError));
    }

    // CBR/UDP flow: n0 to n3
    // PacketSink server on n3
    ns3::PacketSinkHelper sinkHelperUdp1("ns3::UdpSocketFactory",
                                        ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), 9));
    ns3::ApplicationContainer serverAppsUdp1 = sinkHelperUdp1.Install(n3);
    serverAppsUdp1.Start(ns3::Seconds(0.0));
    serverAppsUdp1.Stop(ns3::Seconds(simTime));

    // OnOffApplication client on n0
    // Destination is n3's IP on the n3-n2 link (interfaces_n3n2.GetAddress(0) is n3's address)
    ns3::OnOffHelper onoff1("ns3::UdpSocketFactory",
                           ns3::Address(ns3::InetSocketAddress(interfaces_n3n2.GetAddress(0), 9)));
    onoff1.SetAttribute("OnTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff1.SetAttribute("OffTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=0]")); // For CBR
    onoff1.SetAttribute("PacketSize", ns3::UintegerValue(210));
    onoff1.SetAttribute("DataRate", ns3::StringValue("448Kbps")); // 210 bytes / 0.00375s = 448 Kbps
    ns3::ApplicationContainer clientAppsUdp1 = onoff1.Install(n0);
    clientAppsUdp1.Start(ns3::Seconds(0.1));
    clientAppsUdp1.Stop(ns3::Seconds(simTime - 0.1));

    // CBR/UDP flow: n3 to n1
    // PacketSink server on n1
    ns3::PacketSinkHelper sinkHelperUdp2("ns3::UdpSocketFactory",
                                        ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), 10));
    ns3::ApplicationContainer serverAppsUdp2 = sinkHelperUdp2.Install(n1);
    serverAppsUdp2.Start(ns3::Seconds(0.0));
    serverAppsUdp2.Stop(ns3::Seconds(simTime));

    // OnOffApplication client on n3
    // Destination is n1's IP on the n1-n2 link (interfaces_n1n2.GetAddress(0) is n1's address)
    ns3::OnOffHelper onoff2("ns3::UdpSocketFactory",
                           ns3::Address(ns3::InetSocketAddress(interfaces_n1n2.GetAddress(0), 10)));
    onoff2.SetAttribute("OnTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff2.SetAttribute("OffTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff2.SetAttribute("PacketSize", ns3::UintegerValue(210));
    onoff2.SetAttribute("DataRate", ns3::StringValue("448Kbps"));
    ns3::ApplicationContainer clientAppsUdp2 = onoff2.Install(n3);
    clientAppsUdp2.Start(ns3::Seconds(0.2));
    clientAppsUdp2.Stop(ns3::Seconds(simTime - 0.1));

    // FTP/TCP flow: n0 to n3
    // PacketSink server on n3
    ns3::PacketSinkHelper sinkHelperTcp("ns3::TcpSocketFactory",
                                       ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), 9999));
    ns3::ApplicationContainer sinkAppsTcp = sinkHelperTcp.Install(n3);
    sinkAppsTcp.Start(ns3::Seconds(0.0));
    sinkAppsTcp.Stop(ns3::Seconds(simTime));

    // BulkSendApplication client on n0
    // Destination is n3's IP on the n3-n2 link
    ns3::BulkSendHelper ftpClient("ns3::TcpSocketFactory",
                                 ns3::Address(ns3::InetSocketAddress(interfaces_n3n2.GetAddress(0), 9999)));
    // Set the amount of data to send; 0 means unlimited
    ftpClient.SetAttribute("MaxBytes", ns3::UintegerValue(0));
    ns3::ApplicationContainer clientAppsFtp = ftpClient.Install(n0);
    clientAppsFtp.Start(ns3::Seconds(1.2));
    clientAppsFtp.Stop(ns3::Seconds(1.35));

    // Populate routing tables
    ns3::Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Enable tracing
    ns3::AsciiTraceHelper asciiTraceHelper;
    // Trace queue behaviors and packet receptions for all P2P devices
    p2pHelper5Mbps.EnableAsciiAll(asciiTraceHelper, "simple-error-model.tr");
    p2pHelper1_5Mbps.EnableAsciiAll(asciiTraceHelper, "simple-error-model.tr");

    // Run simulation
    ns3::Simulator::Stop(ns3::Seconds(simTime));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}