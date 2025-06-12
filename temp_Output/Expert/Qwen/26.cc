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

void PrintStats(FlowMonitorHelper* fmHelper, Ptr<FlowMonitor> flowMonitor, std::string runType) {
    std::cout << "\n" << runType << " Statistics:\n";
    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(fmHelper->GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << "):\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000 << " Kbps\n";
    }
}

int main(int argc, char *argv[]) {
    bool enableRtsCts = false;
    CommandLine cmd;
    cmd.Parse(argc, argv);

    for (uint32_t run = 0; run < 2; ++run) {
        RngSeedManager::SetSeed(1);
        RngSeedManager::SetRun(run);

        NodeContainer nodes;
        nodes.Create(3);

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy;
        phy.SetChannel(channel.Create());

        WifiMacHelper mac;
        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211b);
        wifi.SetRemoteStationManager("ns3::ArfWifiManager");

        if (run == 1) {
            wifi.Mac().Set("RtsThreshold", UintegerValue(100));
        } else {
            wifi.Mac().Set("RtsThreshold", UintegerValue(UINT32_MAX));
        }

        mac.SetType("ns3::AdhocWifiMac");
        NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

        MatrixPropagationLossModel lossModel;
        lossModel.SetDefaultLoss(50);
        lossModel.SetLoss(nodes.Get(0)->GetObject<MobilityModel>(), nodes.Get(1)->GetObject<MobilityModel>(), 50);
        lossModel.SetLoss(nodes.Get(1)->GetObject<MobilityModel>(), nodes.Get(0)->GetObject<MobilityModel>(), 50);
        lossModel.SetLoss(nodes.Get(1)->GetObject<MobilityModel>(), nodes.Get(2)->GetObject<MobilityModel>(), 50);
        lossModel.SetLoss(nodes.Get(2)->GetObject<MobilityModel>(), nodes.Get(1)->GetObject<MobilityModel>(), 50);
        lossModel.SetLoss(nodes.Get(0)->GetObject<MobilityModel>(), nodes.Get(2)->GetObject<MobilityModel>(), 50);
        lossModel.SetLoss(nodes.Get(2)->GetObject<MobilityModel>(), nodes.Get(0)->GetObject<MobilityModel>(), 50);

        phy.Set("TxPowerStart", DoubleValue(16));
        phy.Set("TxPowerEnd", DoubleValue(16));
        phy.Set("TxGain", DoubleValue(0));
        phy.Set("RxGain", DoubleValue(0));
        phy.Set("EnergyDetectionThreshold", DoubleValue(-96.0));
        phy.Set("CcaMode1Threshold", DoubleValue(-99.0));

        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(100.0),
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

        OnOffHelper onoff1("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), port)));
        onoff1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        onoff1.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
        onoff1.SetAttribute("PacketSize", UintegerValue(1000));

        ApplicationContainer app1 = onoff1.Install(nodes.Get(0));
        app1.Start(Seconds(1.0));
        app1.Stop(Seconds(10.0));

        OnOffHelper onoff2("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), port)));
        onoff2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        onoff2.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
        onoff2.SetAttribute("PacketSize", UintegerValue(1000));

        ApplicationContainer app2 = onoff2.Install(nodes.Get(2));
        app2.Start(Seconds(1.1));
        app2.Stop(Seconds(10.0));

        FlowMonitorHelper flowmon;
        Ptr<FlowMonitor> monitor = flowmon.InstallAll();

        Simulator::Stop(Seconds(10.0));
        Simulator::Run();

        if (run == 0) {
            PrintStats(&flowmon, monitor, "Without RTS/CTS");
        } else {
            PrintStats(&flowmon, monitor, "With RTS/CTS");
        }

        Simulator::Destroy();
    }

    return 0;
}