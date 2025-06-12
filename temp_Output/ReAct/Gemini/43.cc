#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/vector.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/on-off-application.h"
#include "ns3/udp-client.h"
#include "ns3/udp-server.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    bool enableRtsCts = false;
    uint32_t numAggregatedMpdu = 64;
    uint32_t payloadSize = 1472;
    double simulationTime = 10.0;
    double expectedThroughputMbps = 50.0;

    CommandLine cmd;
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS (0 or 1)", enableRtsCts);
    cmd.AddValue("numAggregatedMpdu", "Number of aggregated MPDUs", numAggregatedMpdu);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("expectedThroughputMbps", "Expected throughput in Mbps", expectedThroughputMbps);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiMac::FragmentThreshold", StringValue("2200"));
    Config::SetDefault("ns3::WifiMac::RtsCtsThreshold", enableRtsCts ? StringValue("0") : StringValue("2200"));

    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(2);

    YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default();

    phyHelper.SetChannel(channelHelper.Create());
    channelHelper.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(5.0));
    channelHelper.AddPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");

    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211n);

    WifiMacHelper macHelper;

    Ssid ssid = Ssid("ns3-mpdu-aggregation");
    macHelper.SetType("ns3::ApWifiMac",
                      "Ssid", SsidValue(ssid),
                      "BeaconInterval", TimeValue(MilliSeconds(100)));

    NetDeviceContainer apDevice = wifiHelper.Install(phyHelper, macHelper, apNode);

    macHelper.SetType("ns3::StaWifiMac",
                      "Ssid", SsidValue(ssid),
                      "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifiHelper.Install(phyHelper, macHelper, staNodes);

    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MpduAggregation/MaxAmsduSize", UintegerValue(numAggregatedMpdu));

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(2.0),
                                  "DeltaY", DoubleValue(2.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    mobility.Install(staNodes);

    InternetStackHelper stackHelper;
    stackHelper.Install(apNode);
    stackHelper.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;

    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(apNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simulationTime + 1));

    UdpClientHelper client(apInterface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    client.SetAttribute("Interval", TimeValue(Seconds(0.00001)));
    client.SetAttribute("PacketSize", UintegerValue(payloadSize));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < staNodes.GetN(); ++i) {
        clientApps.Add(client.Install(staNodes.Get(i)));
    }
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime));

    phyHelper.EnablePcapAll("mpdu-aggregation");

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 1));

    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    double totalThroughput = 0.0;
    for (auto i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        if (t.destinationAddress == apInterface.GetAddress(0)) {
            totalThroughput += i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
        }
    }

    Simulator::Destroy();

    std::cout << "Total throughput: " << totalThroughput << " Mbps" << std::endl;

    if (totalThroughput < expectedThroughputMbps * 0.9) {
        std::cerr << "ERROR: Expected throughput not achieved!" << std::endl;
        return 1;
    }

    return 0;
}