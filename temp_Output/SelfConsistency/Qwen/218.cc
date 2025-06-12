#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/dsdv-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VANETDSDVSimulation");

int main(int argc, char *argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes for vehicles
    uint32_t numNodes = 10;
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Setup mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(50.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(10),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes);

    // Set velocity for each node (vehicles moving along the road)
    for (NodeContainer::Iterator it = nodes.Begin(); it != nodes.End(); ++it)
    {
        Ptr<Node> node = *it;
        Ptr<MobilityModel> mob = node->GetObject<MobilityModel>();
        Vector velocity = mob->GetPosition();
        velocity.x = 20.0; // Moving along X-axis at 20 m/s
        mob->SetAttribute("Velocity", VectorValue(Vector(velocity.x, 0, 0)));
    }

    // Setup WiFi
    WifiMacHelper wifiMac;
    WifiPhyHelper wifiPhy;
    wifiPhy.Set("ChannelWidth", UintegerValue(20));

    wifiMac.SetType("ns3::AdhocWifiMac");
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::ArfRateControl");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Install Internet Stack with DSDV routing protocol
    DsdvHelper dsdv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(dsdv);
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup applications: UDP echo server and client
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(numNodes - 1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(numNodes - 1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numNodes - 1; ++i)
    {
        clientApps.Add(echoClient.Install(nodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable packet capture
    wifiPhy.EnablePcapAll("vanet_dsdv_simulation");

    // Flow monitor setup
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();

    // Output flow monitor results
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin(); iter != stats.end(); ++iter)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        NS_LOG_UNCOND("Flow ID: " << iter->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress);
        NS_LOG_UNCOND("  Tx Packets = " << iter->second.txPackets);
        NS_LOG_UNCOND("  Rx Packets = " << iter->second.rxPackets);
        NS_LOG_UNCOND("  Lost Packets = " << iter->second.lostPackets);
        NS_LOG_UNCOND("  Throughput: " << iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1024 << " Kbps");
    }

    Simulator::Destroy();
    return 0;
}