#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocAodvSimulation");

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(10);

    // Configure wireless devices
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Configure mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                  "X", StringValue("250.0"),
                                  "Y", StringValue("250.0"),
                                  "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=250.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 0, 500, 500)));
    mobility.Install(nodes);

    // Install Internet stack and AODV routing
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Create UDP server on node 9
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(nodes.Get(9));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(30.0));

    // Create UDP clients on other nodes
    for (int i = 0; i < 9; ++i) {
        UdpClientHelper client(interfaces.GetAddress(9), port);
        client.SetAttribute("MaxPackets", UintegerValue(1000));
        client.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
        client.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = client.Install(nodes.Get(i));
        clientApp.Start(Seconds(2.0 + (double)rand()/RAND_MAX * 5.0));
        clientApp.Stop(Seconds(30.0));
    }

    // Enable PCAP tracing
    wifiPhy.EnablePcapAll("adhoc-aodv");

    // Flow Monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Run simulation
    Simulator::Stop(Seconds(30.0));
    Simulator::Run();

    // Calculate and print metrics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double totalPDR = 0.0;
    double totalDelay = 0.0;
    int flowCount = 0;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        if (t.destinationAddress == interfaces.GetAddress(9)) {
            totalPDR += (double)i->second.rxPackets / (double)i->second.txPackets;
            totalDelay += (double)i->second.delaySum.GetSeconds() / (double)i->second.rxPackets;
            flowCount++;
        }
    }

    double avgPDR = totalPDR / flowCount;
    double avgDelay = totalDelay / flowCount;

    std::cout << "Average Packet Delivery Ratio: " << avgPDR << std::endl;
    std::cout << "Average End-to-End Delay: " << avgDelay << " seconds" << std::endl;

    Simulator::Destroy();
    return 0;
}