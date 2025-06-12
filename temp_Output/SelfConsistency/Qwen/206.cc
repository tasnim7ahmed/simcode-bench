#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/mobility-module.h"
#include "ns3/animation-interface.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ieee802154-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SensorNetworkSimulation");

int main(int argc, char *argv[])
{
    // Log components
    LogComponentEnable("SensorNetworkSimulation", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(6); // 5 sensors + 1 sink

    // Create channel and device helpers
    Ieee802154Helper ieee802154 = Ieee802154Helper::Default();
    ieee802154.SetDeviceAttribute("RadioChannelPage", UintegerValue(0));
    ieee802154.SetDeviceAttribute("RadioPanId", UintegerValue(0));
    ieee802154.SetDeviceAttribute("RadioTxPower", DoubleValue(0.0));
    ieee802154.SetDeviceAttribute("RadioCcaMode1Threshold", DoubleValue(-75.0));

    // Install devices
    NetDeviceContainer devices = ieee802154.Install(nodes);
    
    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(6),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // UDP server on sink node (node 0)
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP clients on sensor nodes (nodes 1-5)
    UdpClientHelper client(interfaces.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    client.SetAttribute("PacketSize", UintegerValue(512));

    for (uint32_t i = 1; i < 6; ++i)
    {
        ApplicationContainer clientApp = client.Install(nodes.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));
    }

    // Enable ASCII tracing
    AsciiTraceHelper ascii;
    ieee802154.EnableAsciiAll(ascii.CreateFileStream("sensor-network.tr"));

    // Setup FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Set up animation
    AnimationInterface anim("sensor-network.xml");
    anim.SetConstantPosition(nodes.Get(0), 0, 0); // Sink at center
    for (uint32_t i = 1; i < 6; ++i)
    {
        double angle = 2 * M_PI * (i - 1) / 5;
        double x = 20.0 * cos(angle);
        double y = 20.0 * sin(angle);
        anim.SetConstantPosition(nodes.Get(i), x, y);
    }

    // Simulation setup
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Output flow statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowStats> stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowStats>::const_iterator iter = stats.begin(); iter != stats.end(); ++iter)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        if (t.sourcePort == port)
        {
            NS_LOG_INFO("Flow ID: " << iter->first << " Src Addr: " << t.sourceAddress <<
                        " Dst Addr: " << t.destinationAddress <<
                        " Packets Delivered: " << iter->second.rxPackets <<
                        " Lost Packets: " << iter->second.lostPackets <<
                        " Average Delay: " << iter->second.delaySum.GetSeconds() / iter->second.rxPackets <<
                        " Throughput: " << iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1024 << " Kbps");
        }
    }

    Simulator::Destroy();
    return 0;
}