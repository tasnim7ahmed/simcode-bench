#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/stats-module.h"
#include "ns3/error-model.h"

using namespace ns3;

int main(int argc, char *argv[]) {

    bool verbose = false;
    bool tracing = false;
    bool pcapTracing = false;
    double simulationTime = 10.0;
    double distance = 10.0;
    uint32_t mcsIndex = 7;
    uint32_t channelWidthIndex = 0;
    uint32_t guardIntervalIndex = 0;
    double waveformPower = 0.0;
    std::string wifiType = "80211n";
    double errorRate = 0.0;

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log if true.", verbose);
    cmd.AddValue("tracing", "Enable packet tracing", tracing);
    cmd.AddValue("pcap", "Enable PCAP tracing", pcapTracing);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("distance", "Distance between nodes", distance);
    cmd.AddValue("mcs", "MCS index", mcsIndex);
    cmd.AddValue("channelWidth", "Channel width index", channelWidthIndex);
    cmd.AddValue("guardInterval", "Guard interval index", guardIntervalIndex);
    cmd.AddValue("waveformPower", "Waveform power", waveformPower);
    cmd.AddValue("wifiType", "Wi-Fi type (80211n, 80211ac)", wifiType);
    cmd.AddValue("errorRate", "Packet error rate", errorRate);

    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
        LogComponentEnable("WifiPhy", LOG_LEVEL_ALL);
    }

    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    NodeContainer interferenceNode;
    interferenceNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiNodes.Get(1));

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(Seconds(0.1)));

    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, mac, wifiNodes.Get(0));

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    MobilityHelper interferenceMobility;
    interferenceMobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                              "X", DoubleValue(distance / 2),
                                              "Y", DoubleValue(0),
                                              "Rho", DoubleValue(distance / 4));
    interferenceMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    interferenceMobility.Install(interferenceNode);

    InternetStackHelper stack;
    stack.Install(wifiNodes);
    stack.Install(interferenceNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevices);

    Ipv4AddressHelper interferenceAddress;
    interferenceAddress.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interferenceInterfaces = interferenceAddress.Assign(interferenceNode.GetDevice(0));

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(wifiNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime - 1));

    UdpEchoClientHelper echoClient(apInterface.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(4294967295));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(wifiNodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime - 2));

    if (errorRate > 0.0) {
      Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
      em->SetAttribute("ErrorRate", DoubleValue(errorRate));
      em->SetAttribute("RandomVariable", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=1.0]"));
      staDevices.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    if (tracing) {
        AsciiTraceHelper ascii;
        phy.EnableAsciiAll(ascii.CreateFileStream("wifi-simple-interference.tr"));
    }

    if (pcapTracing) {
      phy.EnablePcapAll("wifi-simple-interference");
    }

    Simulator::Stop(Seconds(simulationTime));

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

    Simulator::Destroy();
    return 0;
}