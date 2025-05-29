#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    bool tracing = true;

    CommandLine cmd;
    cmd.AddValue("tracing", "Enable or disable tracing", tracing);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(3);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, nodes.Get(0));
    staDevices.Add(wifi.Install(phy, mac, nodes.Get(2)));

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, mac, nodes.Get(1));

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(50.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces;
    interfaces = address.Assign(staDevices);
    interfaces.Add(address.Assign(apDevices));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(2), port));
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(2));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    Ptr<UniformRandomVariable> random = CreateObject<UniformRandomVariable>();
    random->SetAttribute("Min", DoubleValue(0.0));
    random->SetAttribute("Max", DoubleValue(1.0));

    OnOffHelper onOffHelper("ns3::UdpSocketFactory", sinkAddress);
    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffHelper.SetAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    ApplicationContainer onOffApp = onOffHelper.Install(nodes.Get(0));
    onOffApp.Start(Seconds(2.0));
    onOffApp.Stop(Seconds(10.0));

    if (tracing) {
        phy.EnablePcapAll("wifi-simple-adhoc");
    }

    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    Simulator::Stop(Seconds(11.0));

    AnimationInterface anim("wifi-simple-adhoc.xml");
    anim.SetConstantPosition(nodes.Get(0), 0.0, 0.0);
    anim.SetConstantPosition(nodes.Get(1), 50.0, 0.0);
    anim.SetConstantPosition(nodes.Get(2), 100.0, 0.0);

    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
    }

    monitor->SerializeToXmlFile("wifi-simple-adhoc.flowmon", true, true);

    Simulator::Destroy();

    return 0;
}