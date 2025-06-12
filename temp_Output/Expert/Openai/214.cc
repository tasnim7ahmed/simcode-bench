#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiApStaSimulation");

void
ThroughputStats (FlowMonitorHelper *flowHelper, Ptr<FlowMonitor> monitor)
{
    monitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowHelper->GetClassifier ());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
    double totalThru = 0.0;
    uint64_t totalRx = 0;
    uint64_t totalTx = 0;
    uint64_t totalLost = 0;

    for (auto &it : stats) {
        totalRx += it.second.rxPackets;
        totalTx += it.second.txPackets;
        totalLost += it.second.lostPackets;
        double time = (it.second.timeLastRxPacket.GetSeconds () - it.second.timeFirstTxPacket.GetSeconds ());
        double thr = (it.second.rxBytes * 8.0) / (time > 0 ? time : 1); // bits/sec
        totalThru += thr;
    }

    std::cout << "========== Throughput & Packet Loss ==========\n";
    std::cout << "Total TX Packets: " << totalTx << std::endl;
    std::cout << "Total RX Packets: " << totalRx << std::endl;
    std::cout << "Total Lost Packets: " << totalLost << std::endl;
    std::cout << "Packet Loss Ratio: ";
    if (totalTx > 0)
        std::cout << 100.0 * totalLost / totalTx << " %\n";
    else
        std::cout << "N/A\n";
    std::cout << "Total Throughput: " << totalThru / 1e6 << " Mbps\n";
    std::cout << "==============================================" << std::endl;
}

int
main (int argc, char *argv[])
{
    LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

    uint32_t nSta = 3;
    uint32_t nAp = 1;
    double simulationTime = 20.0;
    uint32_t packetSize = 1024;
    double dataRate = 10.0; // Mbps

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create (nSta);
    NodeContainer wifiApNode;
    wifiApNode.Create (nAp);

    NodeContainer allNodes;
    allNodes.Add (wifiStaNodes);
    allNodes.Add (wifiApNode);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    phy.SetChannel (channel.Create ());

    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211ac);

    WifiMacHelper mac;
    Ssid ssid = Ssid ("ns3-wifi-ssid");
    mac.SetType ("ns3::StaWifiMac",
                 "Ssid", SsidValue (ssid),
                 "ActiveProbing", BooleanValue (false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install (phy, mac, wifiStaNodes);

    mac.SetType ("ns3::ApWifiMac",
                 "Ssid", SsidValue (ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install (phy, mac, wifiApNode);

    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue (0.0),
                                  "MinY", DoubleValue (0.0),
                                  "DeltaX", DoubleValue (5.0),
                                  "DeltaY", DoubleValue (5.0),
                                  "GridWidth", UintegerValue (2),
                                  "LayoutType", StringValue ("RowFirst"));
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (allNodes);

    InternetStackHelper stack;
    stack.Install (allNodes);

    Ipv4AddressHelper address;
    address.SetBase ("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign (staDevices);
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign (apDevice);

    // Set up UDP Servers on each STA node
    ApplicationContainer serverApps;
    for (uint32_t i = 0; i < nSta; ++i)
    {
        uint16_t port = 8000 + i;
        UdpServerHelper server (port);
        serverApps.Add(server.Install (wifiStaNodes.Get (i)));
    }

    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (simulationTime));

    // Client applications: for full mesh between STAs, each STA sends to every other
    ApplicationContainer clientApps;
    for (uint32_t src = 0; src < nSta; ++src)
    {
        for (uint32_t dst = 0; dst < nSta; ++dst)
        {
            if (src == dst) continue;
            uint16_t port = 8000 + dst;
            UdpClientHelper client (staInterfaces.GetAddress (dst), port);
            client.SetAttribute ("MaxPackets", UintegerValue (1000000));
            client.SetAttribute ("Interval", TimeValue (Seconds (0.005))); // 200 packets/s
            client.SetAttribute ("PacketSize", UintegerValue (packetSize));
            client.SetAttribute ("StartTime", TimeValue (Seconds (2.0 + 0.1 * (src*nSta+dst))));
            client.SetAttribute ("StopTime", TimeValue (Seconds (simulationTime)));
            clientApps.Add(client.Install (wifiStaNodes.Get (src)));
        }
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    phy.EnablePcapAll ("wifi-ap-sta");

    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> monitor = flowHelper.InstallAll ();

    Simulator::Stop (Seconds (simulationTime + 1));
    Simulator::Run ();

    ThroughputStats (&flowHelper, monitor);

    Simulator::Destroy ();
    return 0;
}