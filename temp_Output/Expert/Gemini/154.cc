#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include <fstream>

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(5);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, nodes.Get(0));
    for (uint32_t i = 1; i < 4; ++i) {
        staDevices.Add(wifi.Install(phy, mac, nodes.Get(i)));
    }

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevices = wifi.Install(phy, mac, nodes.Get(4));

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer::Create(staDevices, apDevices));

    UdpClientHelper client(interfaces.GetAddress(4), 9);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.001)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < 4; ++i) {
        clientApps.Add(client.Install(nodes.Get(i)));
    }

    UdpServerHelper server(9);
    ApplicationContainer serverApps = server.Install(nodes.Get(4));

    clientApps.Start(Seconds(1.0));
    serverApps.Start(Seconds(1.0));

    Simulator::Stop(Seconds(10.0));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Run();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    std::ofstream csvFile("simulation_data.csv");

    csvFile << "Source Node,Destination Node,Packet Size,Transmission Time,Reception Time" << std::endl;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        for (uint32_t j = 0; j < i->second.txPackets; ++j) {
            csvFile << t.sourceAddress << "," << t.destinationAddress << ","
                    << i->second.packetSize << "," << i->second.timeFirstTxPacket.GetSeconds() << ","
                    << i->second.timeLastRxPacket.GetSeconds() << std::endl;
        }
    }

    csvFile.close();
    monitor->SerializeToXmlFile("simulation.flowmon", true, true);

    Simulator::Destroy();
    return 0;
}