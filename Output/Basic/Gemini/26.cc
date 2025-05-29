#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    bool enableRtsCts = false;

    CommandLine cmd;
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS", enableRtsCts);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(3);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Create();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    if (enableRtsCts) {
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_Txop/RtsCtsThreshold", UintegerValue(100));
        std::cout << "RTS/CTS Enabled" << std::endl;
    } else {
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_Txop/RtsCtsThreshold", UintegerValue(2200));
        std::cout << "RTS/CTS Disabled" << std::endl;
    }

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    OnOffHelper onoff1("ns3::UdpClient", Address(InetSocketAddress(interfaces.GetAddress(1), 9)));
    onoff1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff1.SetAttribute("PacketSize", UintegerValue(1472));
    onoff1.SetAttribute("DataRate", StringValue("5Mbps"));
    ApplicationContainer clientApps1 = onoff1.Install(nodes.Get(0));
    clientApps1.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(10.0));

    OnOffHelper onoff2("ns3::UdpClient", Address(InetSocketAddress(interfaces.GetAddress(1), 9)));
    onoff2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff2.SetAttribute("PacketSize", UintegerValue(1472));
    onoff2.SetAttribute("DataRate", StringValue("5Mbps"));
    ApplicationContainer clientApps2 = onoff2.Install(nodes.Get(2));
    clientApps2.Start(Seconds(2.1));
    clientApps2.Stop(Seconds(10.0));

    Ptr<MatrixPropagationLossModel> lossModel = CreateObject<MatrixPropagationLossModel>();
    lossModel->SetDefaultLoss(50.0);
    phy.SetPropagationLossModel(lossModel);

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));

    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
    }

    Simulator::Destroy();

    return 0;
}