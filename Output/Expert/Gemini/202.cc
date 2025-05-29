#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-loss-model.h"
#include <cmath>

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::OnOffApplication::PacketSize", UintegerValue(1472));
    Config::SetDefault("ns3::OnOffApplication::DataRate", StringValue("100kbps"));

    NodeContainer nodes;
    nodes.Create(6);

    LrWpanHelper lrWpanHelper;
    NetDeviceContainer lrWpanDevices = lrWpanHelper.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(lrWpanDevices);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(50));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.2)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < nodes.GetN(); ++i) {
        clientApps.Add(echoClient.Install(nodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::CirclePositionAllocator",
                                 "Radius", DoubleValue (5.0),
                                 "Center", VectorValue (0.0, 0.0, 0.0));
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (nodes);

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

    AnimationInterface anim("wpan-animation.xml");
    anim.SetConstantPosition(nodes.Get(0), 0.0, 0.0);
    anim.SetConstantPosition(nodes.Get(1), 5.0, 0.0);
    anim.SetConstantPosition(nodes.Get(2), 2.5, 4.33);
    anim.SetConstantPosition(nodes.Get(3), -2.5, 4.33);
    anim.SetConstantPosition(nodes.Get(4), -5.0, 0.0);
    anim.SetConstantPosition(nodes.Get(5), -2.5, -4.33);

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();

    monitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
        std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024/1024  << " Mbps\n";
        std::cout << "  End-to-End Delay: " << i->second.delaySum.GetSeconds() / i->second.rxPackets << " s\n";

    }

    monitor->SerializeToXmlFile("wpan-flowmon.xml", true, true);

    Simulator::Destroy();
    return 0;
}