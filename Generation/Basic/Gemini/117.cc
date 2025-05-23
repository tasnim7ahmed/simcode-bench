#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

int main(int argc, char *argv[])
{
    ns3::LogComponentEnable("UdpEchoClientApplication", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("UdpEchoServerApplication", ns3::LOG_LEVEL_INFO);

    double simulationTime = 30.0; // seconds
    uint32_t maxPackets = 50;
    double packetInterval = 0.5; // seconds
    uint32_t packetSize = 1024; // bytes

    ns3::CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Total duration of the simulation (seconds)", simulationTime);
    cmd.AddValue("maxPackets", "Maximum number of packets for UDP echo clients", maxPackets);
    cmd.AddValue("packetInterval", "Interval between UDP echo packets (seconds)", packetInterval);
    cmd.AddValue("packetSize", "Size of UDP echo packets (bytes)", packetSize);
    cmd.Parse(argc, argv);

    ns3::RngSeedManager::SetSeed(1);
    ns3::RngSeedManager::SetRun(7);

    ns3::NodeContainer nodes;
    nodes.Create(2);

    ns3::YansWifiChannelHelper channel;
    channel.SetPropagationLossModel("ns3::LogDistancePropagationLossModel",
                                    "Exponent", ns3::StringValue("3.0"));
    channel.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");

    ns3::YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    phy.Set("TxPower", ns3::DoubleValue(50.0));
    phy.SetStandard(ns3::WIFI_PHY_STANDARD_80211a);

    ns3::WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_PHY_STANDARD_80211a);
    ns3::NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    ns3::MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", ns3::RectangleValue(ns3::Rectangle(-1000, 1000, -1000, 1000)),
                              "Speed", ns3::StringValue("ns3::ConstantRandomVariable[Constant=10.0]"),
                              "Pause", ns3::StringValue("ns3::ConstantRandomVariable[Constant=0.1]"));
    mobility.Install(nodes);

    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    ns3::Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = address.Assign(devices);

    ns3::UdpEchoServerHelper echoServer0(9);
    ns3::ApplicationContainer serverApps0 = echoServer0.Install(nodes.Get(0));
    serverApps0.Start(ns3::Seconds(1.0));
    serverApps0.Stop(ns3::Seconds(simulationTime));

    ns3::UdpEchoClientHelper echoClient0(interfaces.GetAddress(1), 9);
    echoClient0.SetAttribute("MaxPackets", ns3::UintegerValue(maxPackets));
    echoClient0.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(packetInterval)));
    echoClient0.SetAttribute("PacketSize", ns3::UintegerValue(packetSize));
    ns3::ApplicationContainer clientApps0 = echoClient0.Install(nodes.Get(0));
    clientApps0.Start(ns3::Seconds(1.0));
    clientApps0.Stop(ns3::Seconds(simulationTime));

    ns3::UdpEchoServerHelper echoServer1(9);
    ns3::ApplicationContainer serverApps1 = echoServer1.Install(nodes.Get(1));
    serverApps1.Start(ns3::Seconds(1.0));
    serverApps1.Stop(ns3::Seconds(simulationTime));

    ns3::UdpEchoClientHelper echoClient1(interfaces.GetAddress(0), 9);
    echoClient1.SetAttribute("MaxPackets", ns3::UintegerValue(maxPackets));
    echoClient1.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(packetInterval)));
    echoClient1.SetAttribute("PacketSize", ns3::UintegerValue(packetSize));
    ns3::ApplicationContainer clientApps1 = echoClient1.Install(nodes.Get(1));
    clientApps1.Start(ns3::Seconds(1.0));
    clientApps1.Stop(ns3::Seconds(simulationTime));

    ns3::Ptr<ns3::FlowMonitor> flowMonitor;
    ns3::FlowMonitorHelper flowMonitorHelper;
    flowMonitor = flowMonitorHelper.Install(nodes);

    ns3::NetAnimHelper netanim;
    netanim.SetOutputFileName("ad-hoc-mobility-animation.xml");
    netanim.EnableAnimation();

    ns3::Simulator::Stop(ns3::Seconds(simulationTime));
    ns3::Simulator::Run();

    flowMonitor->CheckFor = ns3::FlowMonitor::PACKET_DROP;
    flowMonitor->SerializeToXmlFile("ad-hoc-flow-monitor.xml", true, true);

    ns3::Ptr<ns3::Ipv4FlowClassifier> classifier = ns3::DynamicCast<ns3::Ipv4FlowClassifier>(flowMonitorHelper.GetClassifier());
    std::map<ns3::FlowId, ns3::FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

    std::cout << "\n--- Flow Monitor Statistics ---\n";
    for (std::map<ns3::FlowId, ns3::FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
        ns3::Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);

        std::cout << "Flow ID: " << i->first << " (" << t.sourceAddress << ":" << t.sourcePort << " -> " << t.destinationAddress << ":" << t.destinationPort << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Tx Bytes: " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes: " << i->second.rxBytes << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";

        if (i->second.txPackets > 0)
        {
            double packetDeliveryRatio = (double)i->second.rxPackets / i->second.txPackets;
            std::cout << "  Packet Delivery Ratio: " << packetDeliveryRatio * 100.0 << "%\n";
        }
        else
        {
            std::cout << "  Packet Delivery Ratio: N/A (no packets sent)\n";
        }

        if (i->second.rxPackets > 0 && (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) > 0)
        {
            double throughput = (i->second.rxBytes * 8.0) / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024; // Mbps
            std::cout << "  Throughput: " << throughput << " Mbps\n";
            std::cout << "  End-to-End Delay: " << i->second.delaySum.GetSeconds() / i->second.rxPackets << " seconds\n";
        }
        else
        {
            std::cout << "  Throughput: N/A\n";
            std::cout << "  End-to-End Delay: N/A\n";
        }
        std::cout << "--------------------------------\n";
    }

    ns3::Simulator::Destroy();

    return 0;
}