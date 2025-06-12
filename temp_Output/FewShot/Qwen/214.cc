#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for Wi-Fi and applications
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("WifiMacQueueItem", LOG_LEVEL_DEBUG);
    LogComponentEnable("FlowMonitor", LOG_LEVEL_INFO);

    // Create 4 nodes: one AP and three STAs
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(3);

    // Combine all nodes
    NodeContainer allNodes = NodeContainer(wifiApNode, wifiStaNodes);

    // Install Wi-Fi devices with default MAC and PHY
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5MHz);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    // Set up AP
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(Seconds(2.5)));
    NetDeviceContainer apDevice = wifi.Install(mac, wifiApNode.Get(0));

    // Set up STAs
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(mac, wifiStaNodes);

    // Install internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(allNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // Set up mobility (static)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(4),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(allNodes);

    // Set up applications
    uint16_t port = 9;

    // Use first STA as server
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(wifiStaNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Second and third STAs as clients sending to the first STA
    UdpEchoClientHelper echoClient1(staInterfaces.GetAddress(0), port);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(20));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp1 = echoClient1.Install(wifiStaNodes.Get(1));
    clientApp1.Start(Seconds(2.0));
    clientApp1.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient2(staInterfaces.GetAddress(0), port);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(20));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp2 = echoClient2.Install(wifiStaNodes.Get(2));
    clientApp2.Start(Seconds(2.0));
    clientApp2.Stop(Seconds(10.0));

    // Enable PCAP tracing
    wifi.EnablePcapAll("wifi-network-trace");

    // Setup FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.Install(allNodes);

    // Run simulation
    Simulator::Run();

    // Print performance metrics
    monitor->CheckForLostPackets();
    std::cout << "\nPerformance Metrics:\n";
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (auto it = stats.begin(); it != stats.end(); ++it) {
        FlowId id = it->first;
        FlowMonitor::FlowStats st = it->second;
        std::cout << "Flow ID: " << id << " (" 
                  << interfaces.GetAddress(0, 0) << " -> " << interfaces.GetAddress(1, 0) << ")\n";
        std::cout << "  Tx Packets: " << st.txPackets << "\n";
        std::cout << "  Rx Packets: " << st.rxPackets << "\n";
        std::cout << "  Lost Packets: " << st.lostPackets << "\n";
        std::cout << "  Throughput: " << st.rxBytes * 8.0 / (st.timeLastRxPacket.GetSeconds() - st.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
    }

    Simulator::Destroy();
    return 0;
}