#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AODVAdhocNetwork");

int main(int argc, char *argv[])
{
    // Set up logging for AODV
    LogComponentEnable("AodvRoutingProtocol", LOG_LEVEL_INFO);

    // Create 5 nodes
    NodeContainer nodes;
    nodes.Create(5);

    // Set up wireless channel and PHY layer
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Configure WiFi (non-infrastructure mode)
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Set mobility model (random placement of nodes)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install internet stack with AODV routing protocol
    AodvHelper aodv;
    Ipv4ListRoutingHelper list;
    list.Add(aodv, 100);

    InternetStackHelper internet;
    internet.SetRoutingHelper(list);
    internet.Install(nodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Set up UDP Echo Server on node 1
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Set up UDP Echo Client on node 0 to send packets to node 1
    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable pcap tracing
    phy.EnablePcap("aodv-adhoc", devices);

    // Set up flow monitor to track network performance
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Print flow monitor statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        NS_LOG_UNCOND("Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")");
        NS_LOG_UNCOND("  Tx Packets: " << i->second.txPackets);
        NS_LOG_UNCOND("  Rx Packets: " << i->second.rxPackets);
        NS_LOG_UNCOND("  Throughput: " << i->second.rxBytes * 8.0 /
                                            (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) /
                                            1024 / 1024 << " Mbps");
    }

    // Clean up and exit
    Simulator::Destroy();
    return 0;
}

