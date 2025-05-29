#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/command-line.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    bool enableRtsCts = false;
    uint32_t numAggregatedMpdu = 1;
    uint32_t payloadSize = 1472;
    double simulationTime = 10.0;
    uint32_t minThrougput = 5;

    CommandLine cmd;
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS (0 or 1)", enableRtsCts);
    cmd.AddValue("numAggregatedMpdu", "Number of aggregated MPDUs", numAggregatedMpdu);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("minThrougput", "Minimal expected throughput in Mb/s", minThrougput);
    cmd.Parse(argc, argv);

    NodeContainer apNode;
    apNode.Create(1);

    NodeContainer staNodes;
    staNodes.Create(2);

    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211n);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("ns3-80211n");

    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevice = wifiHelper.Install(wifiPhy, wifiMac, apNode);

    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifiHelper.Install(wifiPhy, wifiMac, staNodes);

    if (enableRtsCts) {
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/Mpwd/RtsCtsThreshold", StringValue("0"));
    }

    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MpduAggregation/MaxAmsduSize", StringValue("7935"));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MpduAggregation/MaxAmpduSize", StringValue("65535"));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MpduAggregation/FramesPerAmpdu", UintegerValue(numAggregatedMpdu));

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(staNodes.Get(0));
    mobility.Get(staNodes.Get(0))->SetPosition(Vector(1.0, 1.0, 0.0));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(staNodes.Get(1));
    mobility.Get(staNodes.Get(1))->SetPosition(Vector(10.0, 1.0, 0.0));

    InternetStackHelper internet;
    internet.Install(apNode);
    internet.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;

    UdpClientHelper client1(apInterface.GetAddress(0), port);
    client1.SetAttribute("MaxPackets", UintegerValue(4294967295));
    client1.SetAttribute("Interval", TimeValue(Seconds(0.00001)));
    client1.SetAttribute("PacketSize", UintegerValue(payloadSize));
    ApplicationContainer clientApps1 = client1.Install(staNodes.Get(0));
    clientApps1.Start(Seconds(1.0));
    clientApps1.Stop(Seconds(simulationTime + 1));

    UdpClientHelper client2(apInterface.GetAddress(0), port);
    client2.SetAttribute("MaxPackets", UintegerValue(4294967295));
    client2.SetAttribute("Interval", TimeValue(Seconds(0.00001)));
    client2.SetAttribute("PacketSize", UintegerValue(payloadSize));
    ApplicationContainer clientApps2 = client2.Install(staNodes.Get(1));
    clientApps2.Start(Seconds(1.0));
    clientApps2.Stop(Seconds(simulationTime + 1));

    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(apNode);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime + 1));


    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 2));

    wifiPhy.EnablePcapAll("hidden-stations");
    AsciiTraceHelper ascii;
    wifiPhy.EnableAsciiAll(ascii.CreateFileStream("hidden-stations.tr"));

    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double total_throughput = 0.0;
    for (auto i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        if (t.destinationAddress == apInterface.GetAddress(0)) {
            double throughput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
            std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
            std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
            std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
            std::cout << "  Throughput: " << throughput << " Mb/s\n";
            total_throughput += throughput;
        }
    }

    std::cout << "Total throughput: " << total_throughput << " Mb/s" << std::endl;

    if (total_throughput < minThrougput) {
        std::cerr << "Error: Expected throughput " << minThrougput << " Mb/s or greater" << std::endl;
        exit(1);
    }

    monitor->SerializeToXmlFile("hidden-stations.flowmon", true, true);

    Simulator::Destroy();

    return 0;
}