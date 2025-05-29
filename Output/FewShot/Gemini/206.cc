#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    uint32_t numNodes = 5;
    double simulationTime = 10.0;
    std::string phyMode("OfdmRate6Mbps");

    NodeContainer sinkNode;
    sinkNode.Create(1);

    NodeContainer sensorNodes;
    sensorNodes.Create(numNodes);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue(phyMode),
                                "ControlMode", StringValue(phyMode));

    WifiMacHelper mac;
    Ssid ssid = Ssid("sensor-network");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer sensorDevices = wifi.Install(phy, mac, sensorNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(Seconds(1.0)));

    NetDeviceContainer sinkDevice = wifi.Install(phy, mac, sinkNode);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(numNodes));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(sensorNodes);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(sinkNode);

    InternetStackHelper stack;
    stack.Install(sinkNode);
    stack.Install(sensorNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer sinkInterface = address.Assign(sinkDevice);
    Ipv4InterfaceContainer sensorInterfaces = address.Assign(sensorDevices);

    uint16_t port = 9;
    UdpClientServerHelper clientServer(port);
    clientServer.SetAttribute("MaxPackets", UintegerValue(1000000));
    clientServer.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    clientServer.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numNodes; ++i) {
        clientServer.SetAttribute("RemoteAddress", AddressValue(sinkInterface.GetAddress(0)));
        clientApps.Add(clientServer.Install(sensorNodes.Get(i)));
    }

    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime));

    AnimationInterface anim("sensor-network.xml");
    anim.SetConstantPosition(sinkNode.Get(0), 0.0, 0.0);
    for(uint32_t i = 0; i < numNodes; ++i){
        anim.SetConstantPosition(sensorNodes.Get(i), (i%3 + 1)*5.0, (i/3 + 1)*5.0);
    }

    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 1));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    for (auto i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        if (t.destinationAddress == sinkInterface.GetAddress(0)) {
            std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
            std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
            std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
            std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps\n";
            std::cout << "  Delay Sum:  " << i->second.delaySum << "\n";
        }
    }

    monitor->SerializeToXmlFile("sensor-network.flowmon", true, true);

    Simulator::Destroy();
    return 0;
}