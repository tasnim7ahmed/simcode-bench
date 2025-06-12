#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/dsr-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VANET_UDP_DSR_SUMO");

int main(int argc, char *argv[]) {
    uint32_t numVehicles = 10;
    double simDuration = 80.0;
    std::string phyMode("DsssRate11Mbps");
    uint32_t packetSize = 1024;
    uint32_t numPackets = 100;
    Time interPacketInterval = Seconds(1.0);
    std::string traciLogFile = "vans.tr";
    std::string mobilityTraceFile = "vans.sumo.xml";

    CommandLine cmd(__FILE__);
    cmd.AddValue("numVehicles", "Number of vehicles in the simulation.", numVehicles);
    cmd.AddValue("simDuration", "Simulation duration in seconds.", simDuration);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue(phyMode));
    Config::SetDefault("ns3::DsrRouting::MaxCacheSize", UintegerValue(100));
    Config::SetDefault("ns3::DsrRouting::RequestTableSize", UintegerValue(30));
    Config::SetDefault("ns3::DsrOptionHeader::MaxSalvageCount", UintegerValue(5));

    NodeContainer nodes;
    nodes.Create(numVehicles);

    WifiMacHelper wifiMac;
    WifiPhyHelper wifiPhy;
    wifiPhy.Set("ChannelWidth", UintegerValue(20));
    wifiPhy.Set("TxPowerStart", DoubleValue(15.0));
    wifiPhy.Set("TxPowerEnd", DoubleValue(15.0));
    wifiPhy.Set("TxGain", DoubleValue(0));
    wifiPhy.Set("RxGain", DoubleValue(0));
    wifiPhy.Set("CCA_Mode1_EdThreshold", DoubleValue(-79.0));
    wifiPhy.Set("PerAntennaModel", BooleanValue(false));
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    wifiMac.SetType("ns3::AdhocWifiMac");
    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode), "ControlMode", StringValue(phyMode));
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    InternetStackHelper internet;
    DsrRoutingHelper dsr;
    internet.SetRoutingHelper(dsr);
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    MobilityHelper mobility;
    TracedCallback<Ptr<const MobilityModel> > posCb;
    mobility.SetMobilityModel("ns3::RandomDirection2DMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 1000, 0, 1000)),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=20]"),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.2]"));
    mobility.EnableAsciiAll(posCb);
    mobility.Install(nodes);

    if (!mobilityTraceFile.empty()) {
        MobilityHelper::EnableLog();
        AsciiTraceHelper ascii;
        Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream(mobilityTraceFile.c_str());
        for (uint32_t i = 0; i < nodes.GetN(); ++i) {
            Ptr<Node> node = nodes.Get(i);
            Vector pos = node->GetObject<MobilityModel>()->GetPosition();
            *stream->GetStream() << "NODE: " << node->GetId() << " POS: " << pos.x << "," << pos.y << std::endl;
        }
        TracedCallback<Ptr<const MobilityModel> > cb;
        cb.ConnectWithoutContext(MakeBoundCallback(&MobilityHelper::CourseChangeCallback, &mobility, stream));
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(100.0),
                                      "DeltaY", DoubleValue(100.0),
                                      "GridWidth", UintegerValue(3),
                                      "LayoutType", StringValue("RowFirst"));
    }

    ApplicationContainer apps;
    for (uint32_t i = 0; i < numVehicles; ++i) {
        for (uint32_t j = 0; j < numVehicles; ++j) {
            if (i != j) {
                UdpEchoServerHelper echoServer(9);
                apps = echoServer.Install(nodes.Get(j));
                apps.Start(Seconds(1.0));
                apps.Stop(Seconds(simDuration - 1.0));

                UdpEchoClientHelper echoClient(interfaces.GetAddress(j, 0), 9);
                echoClient.SetAttribute("MaxPackets", UintegerValue(numPackets));
                echoClient.SetAttribute("Interval", TimeValue(interPacketInterval));
                echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));
                apps = echoClient.Install(nodes.Get(i));
                apps.Start(Seconds(2.0));
                apps.Stop(Seconds(simDuration - 2.0));
            }
        }
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    AnimationInterface anim("vanet_udp_dsr_sumo.xml");
    anim.EnablePacketMetadata(true);
    anim.EnableIpv4RouteTracking("routingtable-wireless.xml", Seconds(0), Seconds(simDuration), Seconds(0.25));

    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = static_cast<Ipv4FlowClassifier*>(flowmon.GetClassifier().Peek())->FindFlow(iter->first);
        std::cout << "Flow ID: " << iter->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << std::endl;
        std::cout << "  Tx Packets: " << iter->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << iter->second.rxPackets << std::endl;
        std::cout << "  Throughput: " << iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1000 << " Kbps" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}