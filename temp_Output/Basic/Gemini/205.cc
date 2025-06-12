#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VehicularNetwork");

int main(int argc, char *argv[]) {
    bool enableNetAnim = true;
    double simulationTime = 10.0;
    std::string phyMode = "OfdmRate6Mbps";
    uint32_t packetSize = 1472;
    uint16_t port = 9;
    double interval = 1.0;

    CommandLine cmd;
    cmd.AddValue("enableNetAnim", "Enable NetAnim visualization", enableNetAnim);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::Wifi80211pHelper::SupportedChannelWidth", UintegerValue(10));

    NodeContainer nodes;
    nodes.Create(4);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211p);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
    wifiPhy.SetChannel(wifiChannel.Create());

    QosWifiMacHelper wifiMac = QosWifiMacHelper::Default();
    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(50.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(4),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes);

    Ptr<ConstantVelocityMobilityModel> cvmm;
    cvmm = nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>();
    Vector velocity(10, 0, 0);
    cvmm->SetVelocity(velocity);

    cvmm = nodes.Get(1)->GetObject<ConstantVelocityMobilityModel>();
    velocity.x = 10;
    cvmm->SetVelocity(velocity);

    cvmm = nodes.Get(2)->GetObject<ConstantVelocityMobilityModel>();
    velocity.x = 10;
    cvmm->SetVelocity(velocity);

    cvmm = nodes.Get(3)->GetObject<ConstantVelocityMobilityModel>();
    velocity.x = 10;
    cvmm->SetVelocity(velocity);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(3));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(3), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime));

    clientApps = echoClient.Install(nodes.Get(1));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime));

    clientApps = echoClient.Install(nodes.Get(2));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime));


    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    if (enableNetAnim) {
        AnimationInterface anim("vehicular-network.xml");
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
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Delay Sum: " << i->second.delaySum << "\n";
        if(i->second.rxPackets > 0){
          std::cout << "  Mean Delay: " << i->second.delaySum.GetSeconds() / i->second.rxPackets << "\n";
        }

    }

    Simulator::Destroy();
    return 0;
}