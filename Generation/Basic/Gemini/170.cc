#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

int main(int argc, char *argv[])
{
    double simulationTime = 10.0;
    std::string phyMode = "DsssRate11Mbps";
    uint32_t payloadSize = 1472;

    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(2);

    YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::FriisPropagationLossModel");

    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    phy.Set("TxPowerStart", DoubleValue(20.0));
    phy.Set("TxPowerEnd", DoubleValue(20.0));
    phy.Set("TxGain", DoubleValue(0));
    phy.Set("RxGain", DoubleValue(0));
    phy.Set("EnergyDetectionThreshold", DoubleValue(-82));
    phy.Set("CcaMode1Threshold", DoubleValue(-79));

    WifiMacHelper apMac;
    Ssid ssid = Ssid("ns3-wifi");
    apMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

    WifiMacHelper staMac;
    staMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue(phyMode),
                                 "ControlMode", StringValue(phyMode));

    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, apMac, apNode);

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, staMac, staNodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(1.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    mobility.Install(staNodes);

    InternetStackHelper internet;
    internet.Install(apNode);
    internet.Install(staNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = ipv4.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = ipv4.Assign(staDevices);

    uint16_t port = 9;

    Address sinkAddress(InetSocketAddress(staInterfaces.GetAddress(0), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp = packetSinkHelper.Install(staNodes.Get(0)); // STA1 is server
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simulationTime + 1));

    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", sinkAddress);
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0));
    bulkSendHelper.SetAttribute("SendSize", UintegerValue(payloadSize));
    ApplicationContainer clientApp = bulkSendHelper.Install(staNodes.Get(1)); // STA2 is client
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simulationTime));

    phy.EnablePcapAll("wifi-1ap-2sta");

    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 1));
    Simulator::Run();

    Ptr<PacketSink> sink = DynamicCast<PacketSink>(serverApp.Get(0));
    double totalRxBytes = sink->GetTotalRxBytes();
    double activeTrafficDuration = clientApp.Get(0)->GetStopTime().GetSeconds() - clientApp.Get(0)->GetStartTime().GetSeconds();
    if (activeTrafficDuration <= 0) {
        activeTrafficDuration = simulationTime;
    }
    double throughputMbps = (totalRxBytes * 8.0) / (activeTrafficDuration * 1000000.0);
    std::cout << "Server Throughput (PacketSink): " << throughputMbps << " Mbps" << std::endl;

    monitor->CheckForConflicts();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    uint32_t totalTxPackets = 0;
    uint32_t totalRxPackets = 0;
    uint32_t totalDroppedPackets = 0;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);

        if (t.sourceAddress == staInterfaces.GetAddress(1) && t.destinationAddress == staInterfaces.GetAddress(0) && t.protocol == 6)
        {
            std::cout << "Flow " << i->first << " (" << t.sourceAddress << ":" << t.sourcePort << " -> " << t.destinationAddress << ":" << t.destinationPort << ")" << std::endl;
            std::cout << "  Tx Packets: " << i->second.txPackets << std::endl;
            std::cout << "  Rx Packets: " << i->second.rxPackets << std::endl;
            std::cout << "  Lost Packets: " << i->second.lostPackets << std::endl;
            std::cout << "  Dropped Packets: " << i->second.packetsDropped.size() << std::endl;
            
            totalTxPackets += i->second.txPackets;
            totalRxPackets += i->second.rxPackets;
            totalDroppedPackets += i->second.packetsDropped.size();
        }
    }

    std::cout << "---------------------------------------------------------" << std::endl;
    std::cout << "Total Packets Transmitted (Client): " << totalTxPackets << std::endl;
    std::cout << "Total Packets Received (Server):    " << totalRxPackets << std::endl;
    std::cout << "Total Packets Dropped (FlowMonitor):" << totalDroppedPackets << std::endl;
    std::cout << "---------------------------------------------------------" << std::endl;
    
    Simulator::Destroy();

    return 0;
}