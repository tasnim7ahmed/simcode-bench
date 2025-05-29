#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiSimpleInfrastructureExample");

int main(int argc, char *argv[])
{
    // Enable logging for packet tracing
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes: 3 STA + 1 AP
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(3);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Configure WiFi
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    WifiMacHelper mac;

    // SSID
    Ssid ssid = Ssid("ns3-wifi");

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

    // Set mobility for all nodes
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));   // STA 0
    positionAlloc->Add(Vector(5.0, 0.0, 0.0));   // STA 1
    positionAlloc->Add(Vector(10.0, 0.0, 0.0));  // STA 2
    positionAlloc->Add(Vector(5.0, 5.0, 0.0));   // AP
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    // Stack and IP assignment
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);

    // Create UDP server on STA 0 and clients on STA 1/STA 2 to test throughput/loss
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(wifiStaNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClient(staInterfaces.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(200));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.05))); // 20 pkts per second
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps1 = echoClient.Install(wifiStaNodes.Get(1));
    clientApps1.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(20.0));

    ApplicationContainer clientApps2 = echoClient.Install(wifiStaNodes.Get(2));
    clientApps2.Start(Seconds(2.0));
    clientApps2.Stop(Seconds(20.0));

    // Enable packet capture
    phy.EnablePcapAll("wifi-infra");

    // Install FlowMonitor to capture throughput and packet loss
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    // Run Simulation
    Simulator::Stop(Seconds(21.0));
    Simulator::Run();

    // FlowMonitor stats
    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

    std::cout << "\n--- FlowMonitor Results ---\n";
    for (const auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << "FlowID: " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
        std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << flow.second.lostPackets << "\n";
        std::cout << "  Throughput: "
                  << flow.second.rxBytes * 8.0 / (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024
                  << " Mbps\n";
    }
    std::cout << "--------------------------\n";

    Simulator::Destroy();
    return 0;
}