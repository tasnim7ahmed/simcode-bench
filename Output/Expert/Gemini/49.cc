#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/yans-wifi-phy.h"
#include "ns3/error-model.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    bool pcapTracing = false;
    double simulationTime = 10.0;
    double distance = 10.0;
    uint32_t mcsIndex = 7;
    uint32_t channelWidthIndex = 0;
    uint32_t guardIntervalIndex = 0;
    double waveformPower = 0.0;
    std::string wifiType = "ns3::YansWifiPhy";
    double errorRate = 0.0;

    CommandLine cmd;
    cmd.AddValue("pcap", "Enable PCAP tracing", pcapTracing);
    cmd.AddValue("time", "Simulation time (s)", simulationTime);
    cmd.AddValue("distance", "Distance between nodes (m)", distance);
    cmd.AddValue("mcs", "MCS index", mcsIndex);
    cmd.AddValue("channelWidth", "Channel width index", channelWidthIndex);
    cmd.AddValue("guardInterval", "Guard interval index", guardIntervalIndex);
    cmd.AddValue("waveformPower", "Waveform power (dBm)", waveformPower);
    cmd.AddValue("wifiType", "Wi-Fi PHY type (YansWifiPhy or SpectrumWifiPhy)", wifiType);
    cmd.AddValue("errorRate", "Error rate for error model", errorRate);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211n);

    YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::FriisPropagationLossModel");

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    if (wifiType == "ns3::SpectrumWifiPhy") {
        phy.Set("TxGain", DoubleValue(0));
        phy.Set("RxGain", DoubleValue(0));
    }

    WifiMacHelper mac;
    Ssid ssid = Ssid("wifi-network");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifiHelper.Install(phy, mac, wifiNodes.Get(1));

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(Seconds(0.1)));

    NetDeviceContainer apDevices;
    apDevices = wifiHelper.Install(phy, mac, wifiNodes.Get(0));

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

    InternetStackHelper stack;
    stack.Install(wifiNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(apDevices);
    interfaces = address.Assign(staDevices);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(wifiNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(4294967295));
    echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(wifiNodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime));

    if (pcapTracing) {
        phy.EnablePcapAll("wifi-simulation");
    }

    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    Simulator::Stop(Seconds(simulationTime));

    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
    }

    Simulator::Destroy();
    return 0;
}