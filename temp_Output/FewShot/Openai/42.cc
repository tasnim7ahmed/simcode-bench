#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-mac-helper.h"
#include "ns3/wifi-phy-helper.h"
#include "ns3/ssid.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("HiddenStation80211nAggregation");

void
ThroughputMonitor (Ptr<PacketSink> sink, double startTime, double simTime, double expectedMin, double expectedMax)
{
    double throughput = (sink->GetTotalRx() * 8.0) / (simTime * 1e6); // Mbps
    std::cout << "Throughput: " << throughput << " Mbps" << std::endl;
    if (expectedMin > 0 && throughput < expectedMin)
    {
        std::cout << "WARNING: Throughput below expected minimum." << std::endl;
    }
    if (expectedMax > 0 && throughput > expectedMax)
    {
        std::cout << "WARNING: Throughput above expected maximum." << std::endl;
    }
}

int main(int argc, char *argv[])
{
    bool enableRtsCts = false;
    uint32_t mpduAggregation = 8;
    uint32_t payloadSize = 1400;
    double simTime = 10.0;
    double expectedThroughputMin = 0.0;
    double expectedThroughputMax = 0.0;
    std::string phyMode = "HtMcs7";

    CommandLine cmd;
    cmd.AddValue ("EnableRtsCts", "Enable/disable RTS/CTS", enableRtsCts);
    cmd.AddValue ("MpduAggregation", "Number of MPDUs per AMPDU", mpduAggregation);
    cmd.AddValue ("PayloadSize", "UDP payload size in bytes", payloadSize);
    cmd.AddValue ("SimTime", "Simulation time in seconds", simTime);
    cmd.AddValue ("ExpectedThroughputMin", "Minimum expected throughput in Mbps", expectedThroughputMin);
    cmd.AddValue ("ExpectedThroughputMax", "Maximum expected throughput in Mbps", expectedThroughputMax);
    cmd.Parse (argc, argv);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel (channel.Create());

    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211n_2_4GHZ);
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue(phyMode),
                                 "ControlMode", StringValue(phyMode));

    WifiMacHelper mac;
    Ssid ssid = Ssid ("hidden-agg-net");

    // Enable MPDU aggregation via WifiMac attributes
    mac.SetType ("ns3::StaWifiMac",
                 "Ssid", SsidValue(ssid),
                 "QosSupported", BooleanValue(true),
                 "BE_MaxAmpduSize", UintegerValue(mpduAggregation * payloadSize));
    NetDeviceContainer staDevices = wifi.Install (phy, mac, wifiStaNodes);

    mac.SetType ("ns3::ApWifiMac",
                 "Ssid", SsidValue(ssid),
                 "QosSupported", BooleanValue(true),
                 "BE_MaxAmpduSize", UintegerValue(mpduAggregation * payloadSize));
    NetDeviceContainer apDevice = wifi.Install (phy, mac, wifiApNode);

    // Enable/disable RTS/CTS
    Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold",
                        UintegerValue (enableRtsCts ? 0 : 999999));

    // Mobility: place AP at origin, n1 and n2 far apart, both within AP range but not within each other's range
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    positionAlloc->Add (Vector (  0.0,     0.0, 0.0)); // AP
    positionAlloc->Add (Vector (-50.0,   300.0, 0.0)); // n1 (sta 0)
    positionAlloc->Add (Vector (+50.0,   300.0, 0.0)); // n2 (sta 1)
    mobility.SetPositionAllocator (positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiApNode);
    mobility.Install (wifiStaNodes);

    InternetStackHelper stack;
    stack.Install (wifiApNode);
    stack.Install (wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase ("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    // UDP servers (sinks) at AP, different ports for each station
    uint16_t port1 = 8000;
    uint16_t port2 = 8001;
    PacketSinkHelper sinkHelper1 ("ns3::UdpSocketFactory", InetSocketAddress (apInterface.GetAddress(0), port1));
    PacketSinkHelper sinkHelper2 ("ns3::UdpSocketFactory", InetSocketAddress (apInterface.GetAddress(0), port2));
    ApplicationContainer sinkApps1 = sinkHelper1.Install (wifiApNode.Get(0));
    ApplicationContainer sinkApps2 = sinkHelper2.Install (wifiApNode.Get(0));
    sinkApps1.Start (Seconds (0.0));
    sinkApps1.Stop (Seconds (simTime + 1));
    sinkApps2.Start (Seconds (0.0));
    sinkApps2.Stop (Seconds (simTime + 1));

    // UDP traffic sources: each station sends to AP
    OnOffHelper udp1 ("ns3::UdpSocketFactory", InetSocketAddress(apInterface.GetAddress(0), port1));
    udp1.SetAttribute ("PacketSize", UintegerValue(payloadSize));
    udp1.SetAttribute ("DataRate", StringValue("54Mbps"));
    udp1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    udp1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer clientApps1 = udp1.Install (wifiStaNodes.Get(0));
    clientApps1.Start (Seconds (1.0));
    clientApps1.Stop (Seconds (simTime + 1));

    OnOffHelper udp2 ("ns3::UdpSocketFactory", InetSocketAddress(apInterface.GetAddress(0), port2));
    udp2.SetAttribute ("PacketSize", UintegerValue(payloadSize));
    udp2.SetAttribute ("DataRate", StringValue("54Mbps"));
    udp2.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    udp2.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer clientApps2 = udp2.Install (wifiStaNodes.Get(1));
    clientApps2.Start (Seconds (1.1));
    clientApps2.Stop (Seconds (simTime + 1));

    // Enable PCAP and ASCII tracing
    phy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.EnablePcap ("hidden-ap", apDevice.Get(0));
    phy.EnablePcap ("hidden-sta1", staDevices.Get(0));
    phy.EnablePcap ("hidden-sta2", staDevices.Get(1));
    AsciiTraceHelper ascii;
    phy.EnableAsciiAll (ascii.CreateFileStream ("hidden-wifi.tr"));

    // Install FlowMonitor to generate trace file and collect stats
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Throughput statistics at end
    Simulator::Stop (Seconds(simTime + 1));
    Simulator::Run();

    Ptr<PacketSink> sink1 = DynamicCast<PacketSink>(sinkApps1.Get(0));
    Ptr<PacketSink> sink2 = DynamicCast<PacketSink>(sinkApps2.Get(0));
    double totalRx = sink1->GetTotalRx() + sink2->GetTotalRx();
    double throughput = (totalRx * 8.0) / (simTime * 1e6);
    std::cout << "Combined throughput: " << throughput << " Mbps" << std::endl;
    if (expectedThroughputMin > 0 && throughput < expectedThroughputMin)
    {
        std::cout << "WARNING: Throughput below expected minimum." << std::endl;
    }
    if (expectedThroughputMax > 0 && throughput > expectedThroughputMax)
    {
        std::cout << "WARNING: Throughput above expected maximum." << std::endl;
    }

    monitor->SerializeToXmlFile ("hidden-wifi-flowmon.xml", true, true);

    Simulator::Destroy();
    return 0;
}