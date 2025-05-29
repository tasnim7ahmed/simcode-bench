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
#include "ns3/nstime.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMCS");

int main(int argc, char *argv[]) {
    bool verbose = false;
    uint32_t payloadSize = 1472;
    std::string dataRate = "5Mbps";
    std::string phyMode = "OfdmRate6Mbps";
    double simulationTime = 10;
    double distance = 10;
    std::string wifiStandard = "802.11a";
    std::string errorModelType = "NistErrorRateModel";

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log if possible", verbose);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("dataRate", "Data rate (e.g., 5Mbps)", dataRate);
    cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("distance", "Distance between nodes", distance);
    cmd.AddValue("wifiStandard", "Wifi standard (e.g., 802.11a)", wifiStandard);
    cmd.AddValue("errorModelType", "Error model type", errorModelType);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(wifiStandard);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevices = wifi.Install(phy, mac, apNode);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    mobility.Install(staNodes);

    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    UdpServerHelper server(9);
    ApplicationContainer serverApps = server.Install(apNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime + 1));

    UdpClientHelper client(apInterfaces.GetAddress(0), 9);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295));
    client.SetAttribute("Interval", TimeValue(Time("0.00002")));
    client.SetAttribute("PacketSize", UintegerValue(payloadSize));

    ApplicationContainer clientApps = client.Install(staNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 1));

    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
    }

    monitor->SerializeToXmlFile("wifi-mcs.flowmon", true, true);

    Simulator::Destroy();

    return 0;
}