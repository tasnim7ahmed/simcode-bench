#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HiddenTerminalRtsCts");

int main(int argc, char *argv[]) {
    bool enableRtsCts = false;
    CommandLine cmd(__FILE__);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS for packets larger than 100 bytes", enableRtsCts);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(3);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    if (enableRtsCts) {
        wifi.MacAttributes().Add("RtsCtsThreshold", UintegerValue(100));
    } else {
        wifi.MacAttributes().Add("RtsCtsThreshold", UintegerValue(999999));
    }

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    WifiPhyHelper phy;
    phy.Set("TxPowerStart", DoubleValue(16));
    phy.Set("TxPowerEnd", DoubleValue(16));
    phy.Set("TxPowerLevels", UintegerValue(1));

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    Ptr<MatrixPropagationLossModel> lossModel = CreateObject<MatrixPropagationLossModel>();
    lossModel->SetDefaultLoss(999); // No direct communication between 0 and 2
    lossModel->SetLoss(nodes.Get(0)->GetId(), nodes.Get(1)->GetId(), 50);
    lossModel->SetLoss(nodes.Get(1)->GetId(), nodes.Get(0)->GetId(), 50);
    lossModel->SetLoss(nodes.Get(1)->GetId(), nodes.Get(2)->GetId(), 50);
    lossModel->SetLoss(nodes.Get(2)->GetId(), nodes.Get(1)->GetId(), 50);
    channel.AddPropagationLoss(lossModel);

    phy.SetChannel(channel.Create());

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;

    OnOffHelper onoff01("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    onoff01.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff01.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff01.SetAttribute("DataRate", DataRateValue(DataRate("100kbps")));
    onoff01.SetAttribute("PacketSize", UintegerValue(1000));

    ApplicationContainer apps01 = onoff01.Install(nodes.Get(0));
    apps01.Start(Seconds(1.0));
    apps01.Stop(Seconds(10.0));

    OnOffHelper onoff21("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    onoff21.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff21.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff21.SetAttribute("DataRate", DataRateValue(DataRate("100kbps")));
    onoff21.SetAttribute("PacketSize", UintegerValue(1000));

    ApplicationContainer apps21 = onoff21.Install(nodes.Get(2));
    apps21.Start(Seconds(1.1)); // Slightly staggered start time
    apps21.Stop(Seconds(10.0));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    std::cout << "Simulation run with RTS/CTS " << (enableRtsCts ? "enabled" : "disabled") << ":" << std::endl;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000 << " kbps\n";
    }

    Simulator::Destroy();

    return 0;
}