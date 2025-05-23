#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

// Function to calculate and output throughput
void CalculateThroughput(Ptr<ns3::PacketSink> sink, double duration)
{
    uint64_t totalRxBytes = sink->GetTotalRx();
    // Convert bytes to Mbits and duration to seconds for Mbps calculation
    double throughputMbps = (totalRxBytes * 8.0) / (duration * 1000000.0);
    ns3::NS_LOG_UNCOND("Total Bytes Received: " << totalRxBytes << " bytes");
    ns3::NS_LOG_UNCOND("Throughput: " << throughputMbps << " Mbps");
}

int main(int argc, char *argv[])
{
    // 1. Simulation parameters
    double totalSimTime = 20.0;     // Total simulation time in seconds
    double clientStartTime = 1.0;   // When client starts sending data
    double clientStopTime = totalSimTime; // When client stops sending data
    // Effective duration for which the client is actively sending data
    double effectiveClientDuration = clientStopTime - clientStartTime;

    uint16_t port = 9;              // TCP port for communication
    uint32_t packetSize = 1024;     // Size of packets sent by OnOffApplication (bytes)
    ns3::DataRate dataRate("10Mbps"); // Data rate for OnOffApplication

    // 2. Configure Time Resolution
    ns3::Time::SetResolution(ns3::NanoSeconds);

    // 3. Create Nodes
    ns3::NodeContainer nodes;
    nodes.Create(2); // Node 0: Server, Node 1: Client

    // 4. Install AODV routing protocol
    ns3::aodv::AodvHelper aodv;
    ns3::InternetStackHelper stack;
    stack.SetRoutingHelper(aodv);
    stack.Install(nodes);

    // 5. Configure Mobility
    ns3::MobilityHelper mobility;
    ns3::Ptr<ns3::ListPositionAllocator> positionAlloc = CreateObject<ns3::ListPositionAllocator>();
    positionAlloc->Add(ns3::Vector(10.0, 10.0, 0.0)); // Node 0 position
    positionAlloc->Add(ns3::Vector(50.0, 10.0, 0.0)); // Node 1 position
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // 6. Configure Wi-Fi
    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_STANDARD_80211n_5GHZ); // Using 802.11n standard at 5GHz band
    wifi.SetRemoteStationManager("ns3::AarfWifiManager"); // AARF for rate adaptation

    ns3::WifiPhyHelper wifiPhy;
    // Optional: for Wireshark pcap capture
    wifiPhy.SetPcapDataLinkType(ns3::WifiPhyHelper::DLT_IEEE802_11_RADIO);

    ns3::YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel"); // Simple propagation loss model
    ns3::Ptr<ns3::YansWifiChannel> channel = wifiChannel.Create();
    wifiPhy.SetChannel(channel);

    ns3::WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac"); // Ad-hoc mode is suitable for AODV
    
    // Install Wi-Fi devices on nodes
    ns3::NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // 7. Assign IP Addresses
    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // 8. Install TCP Applications
    // Server (PacketSink) on Node 0
    ns3::Ptr<ns3::PacketSink> sinkApp;
    ns3::Address sinkAddress(ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), port));
    ns3::PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ns3::ApplicationContainer serverApps = sinkHelper.Install(nodes.Get(0));
    serverApps.Start(ns3::Seconds(0.0));
    serverApps.Stop(ns3::Seconds(totalSimTime));
    sinkApp = DynamicCast<ns3::PacketSink>(serverApps.Get(0)); // Get pointer to the sink application

    // Client (OnOffApplication) on Node 1
    ns3::OnOffHelper onoff("ns3::TcpSocketFactory", ns3::Ipv4Address::GetZero());
    onoff.SetAttribute("OnTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=1]")); // Always On during active period
    onoff.SetAttribute("OffTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=0]")); // No Off period
    onoff.SetAttribute("PacketSize", ns3::UintegerValue(packetSize));
    onoff.SetAttribute("DataRate", ns3::DataRateValue(dataRate));
    // Set remote address to the server's IP (Node 0's IP)
    onoff.SetAttribute("Remote", ns3::AddressValue(ns3::InetSocketAddress(interfaces.GetAddress(0), port)));

    ns3::ApplicationContainer clientApps = onoff.Install(nodes.Get(1));
    clientApps.Start(ns3::Seconds(clientStartTime)); // Client starts at clientStartTime
    clientApps.Stop(ns3::Seconds(clientStopTime));   // Client stops at clientStopTime

    // 9. Enable NetAnim visualization
    ns3::AnimationInterface anim("wifi-aodv-tcp.xml");
    anim.SetConstantPosition(nodes.Get(0), 10.0, 10.0); // Server visualization position
    anim.SetConstantPosition(nodes.Get(1), 50.0, 10.0); // Client visualization position

    // 10. Schedule throughput calculation after simulation ends
    // Schedule it slightly after totalSimTime to ensure all packets are processed
    ns3::Simulator::Schedule(ns3::Seconds(totalSimTime + 0.1), &CalculateThroughput, sinkApp, effectiveClientDuration);

    // 11. Run Simulation
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}