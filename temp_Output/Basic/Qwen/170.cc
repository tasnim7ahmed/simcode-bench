#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiTcpSimulation");

int main (int argc, char *argv[])
{
    CommandLine cmd (__FILE__);
    cmd.Parse (argc, argv);

    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes: AP + 2 STAs
    NodeContainer wifiApNode;
    wifiApNode.Create (1);
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create (2);

    // Setup Wi-Fi
    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211b);

    // Set default algorithm for all nodes
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("DsssRate11Mbps"), "ControlMode", StringValue ("DsssRate11Mbps"));

    // Create channel and phy
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper phy;
    phy.SetChannel (channel.Create ());

    // Install MAC and PHY on AP and STAs
    WifiMacHelper mac;
    Ssid ssid = Ssid ("wifi-network");
    NetDeviceContainer apDevice;
    NetDeviceContainer staDevices;

    // Configure MAC for AP
    mac.SetType ("ns3::ApWifiMac",
                 "Ssid", SsidValue (ssid));
    apDevice = wifi.Install (phy, mac, wifiApNode);

    // Configure MAC for STAs
    mac.SetType ("ns3::StaWifiMac",
                 "Ssid", SsidValue (ssid),
                 "ActiveProbing", BooleanValue (false));
    staDevices = wifi.Install (phy, mac, wifiStaNodes);

    // Mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue (0.0),
                                   "MinY", DoubleValue (0.0),
                                   "DeltaX", DoubleValue (5.0),
                                   "DeltaY", DoubleValue (5.0),
                                   "GridWidth", UintegerValue (3),
                                   "LayoutType", StringValue ("RowFirst"));
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiApNode);
    mobility.Install (wifiStaNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install (wifiApNode);
    stack.Install (wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase ("192.168.1.0", "255.255.255.0");

    Ipv4InterfaceContainer apInterface;
    Ipv4InterfaceContainer staInterfaces;

    apInterface = address.Assign (apDevice);
    staInterfaces = address.Assign (staDevices);

    // TCP traffic setup
    uint16_t port = 50000;

    // Server node is STA 0
    PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
    ApplicationContainer serverApp = packetSinkHelper.Install (wifiStaNodes.Get (0));
    serverApp.Start (Seconds (0.0));
    serverApp.Stop (Seconds (10.0));

    // Client node is STA 1
    OnOffHelper onoff ("ns3::TcpSocketFactory", InetSocketAddress (staInterfaces.GetAddress (0), port));
    onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute ("DataRate", DataRateValue (DataRate ("10Mbps")));
    onoff.SetAttribute ("PacketSize", UintegerValue (1024));

    ApplicationContainer clientApp = onoff.Install (wifiStaNodes.Get (1));
    clientApp.Start (Seconds (1.0));
    clientApp.Stop (Seconds (10.0));

    // PCAP tracing
    phy.EnablePcap ("wifi_ap", apDevice.Get (0));
    phy.EnablePcap ("wifi_sta0", staDevices.Get (0));
    phy.EnablePcap ("wifi_sta1", staDevices.Get (1));

    // Flow monitor setup
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowMonitor = flowmonHelper.InstallAll ();

    Simulator::Stop (Seconds (10.0));
    Simulator::Run ();

    // Measure throughput
    flowMonitor->CheckForLostPackets ();
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats ();

    double totalRxBytes = 0;
    uint64_t totalPackets = 0;
    for (auto it = stats.begin (); it != stats.end (); ++it)
    {
        Ipv4FlowClassifier::FiveTuple t = dynamic_cast<Ipv4FlowClassifier *> (flowmonHelper.GetClassifier ())->FindFlow (it->first);
        if (t.destinationPort == port)
        {
            totalRxBytes += it->second.rxBytes * 8.0 / (it->second.timeLastRxPacket.GetSeconds () - it->second.timeFirstTxPacket.GetSeconds ());
            totalPackets += it->second.rxPackets;
        }
    }

    NS_LOG_UNCOND ("Throughput: " << totalRxBytes / 1e6 << " Mbps");
    NS_LOG_UNCOND ("Total Packets Received: " << totalPackets);

    Simulator::Destroy ();
    return 0;
}