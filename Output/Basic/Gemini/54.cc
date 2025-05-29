#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/spectrum-wifi-phy.h"
#include "ns3/yans-wifi-phy.h"
#include "ns3/wifi-helper.h"
#include "ns3/nqos-wifi-mac-header.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Wifi80211acSimulation");

int main(int argc, char *argv[]) {
    bool useRtsCts = false;
    std::string transportProtocol = "UDP";
    double distance = 1.0;
    uint32_t packetSize = 1472;
    std::string rate = "50Mbps";

    CommandLine cmd;
    cmd.AddValue("useRtsCts", "Enable RTS/CTS (default: false)", useRtsCts);
    cmd.AddValue("transportProtocol", "Transport protocol (UDP or TCP, default: UDP)", transportProtocol);
    cmd.AddValue("distance", "Distance between STA and AP (meters, default: 1.0)", distance);
    cmd.AddValue("packetSize", "Size of packets (bytes, default: 1472)", packetSize);
    cmd.AddValue("rate", "Data rate for OnOff application (default: 50Mbps)", rate);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponent::SetLevel( "UdpClient", LOG_LEVEL_INFO );

    NodeContainer staNodes;
    staNodes.Create(1);
    NodeContainer apNodes;
    apNodes.Create(1);

    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_STANDARD_80211ac);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("ns-3-ssid");
    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifiHelper.Install(wifiPhy, wifiMac, staNodes);

    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid),
                    "BeaconInterval", TimeValue(Seconds(0.05)));

    NetDeviceContainer apDevices = wifiHelper.Install(wifiPhy, wifiMac, apNodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(staNodes);
    mobility.Install(apNodes);

    InternetStackHelper stack;
    stack.Install(staNodes);
    stack.Install(apNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staNodeInterface = address.Assign(staDevices);
    Ipv4InterfaceContainer apNodeInterface = address.Assign(apDevices);

    uint16_t port = 9;

    ApplicationContainer sinkApp;
    if (transportProtocol == "UDP") {
        UdpServerHelper echoServerHelper(port);
        sinkApp = echoServerHelper.Install(apNodes.Get(0));
    } else {
        PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        sinkApp = packetSinkHelper.Install(apNodes.Get(0));
    }

    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    ApplicationContainer clientApp;
    if (transportProtocol == "UDP") {
        UdpClientHelper echoClientHelper(apNodeInterface.GetAddress(0), port);
        echoClientHelper.SetAttribute("MaxPackets", UintegerValue(4294967295));
        echoClientHelper.SetAttribute("Interval", TimeValue(Time(0.00002))); //packets/sec
        echoClientHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
        clientApp = echoClientHelper.Install(staNodes.Get(0));
    } else {
        OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(apNodeInterface.GetAddress(0), port));
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
        onoff.SetAttribute("DataRate", DataRateValue(DataRate(rate)));
        clientApp = onoff.Install(staNodes.Get(0));
    }

    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    if (useRtsCts) {
        Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_Txop/RtsCtsThreshold", UintegerValue (0));
        Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_Txop/FragmentationThreshold", UintegerValue (2200));
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    for (auto i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        if (t.sourceAddress == staNodeInterface.GetAddress(0) && t.destinationAddress == apNodeInterface.GetAddress(0)) {
            std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
            std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
            std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
            std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
            break;
        }
    }

    Simulator::Destroy();
    return 0;
}