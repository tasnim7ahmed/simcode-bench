#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTcpExample");

int main(int argc, char *argv[])
{
    // Set simulation parameters
    double simulationTime = 10.0; // seconds

    // Enable logging for BulkSend and PacketSink applications
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create AP and STA nodes
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2); // Two stations (STA)

    NodeContainer wifiApNode;
    wifiApNode.Create(1); // One access point (AP)

    // Set up Wi-Fi network
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    // Configure STA devices
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Configure AP device
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Set up mobility for stations and AP
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    // Install TCP/IP stack on both AP and STA nodes
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    // Create a TCP server (packet sink) on one STA
    uint16_t port = 9;  // TCP port
    Address sinkAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(wifiStaNodes.Get(1)); // Server on STA2
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(simulationTime + 1));

    // Create a TCP client (bulk sender) on another STA
    BulkSendHelper bulkSend("ns3::TcpSocketFactory",
                            InetSocketAddress(staInterfaces.GetAddress(1), port));
    bulkSend.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited data
    ApplicationContainer clientApp = bulkSend.Install(wifiStaNodes.Get(0)); // Client on STA1
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(simulationTime + 1));

    // Enable packet capture tracing
    phy.EnablePcap("wifi-tcp", apDevice.Get(0));

    // FlowMonitor to monitor traffic
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Run the simulation
    Simulator::Stop(Seconds(simulationTime + 1));
    Simulator::Run();

    // Output flow statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        NS_LOG_UNCOND("Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")");
        NS_LOG_UNCOND("  Tx Bytes: " << i->second.txBytes);
        NS_LOG_UNCOND("  Rx Bytes: " << i->second.rxBytes);
        NS_LOG_UNCOND("  Throughput: " << i->second.rxBytes * 8.0 / simulationTime / 1000 / 1000 << " Mbps");
    }

    Simulator::Destroy();
    return 0;
}

