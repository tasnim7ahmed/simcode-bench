#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocAodvExample");

int main(int argc, char *argv[])
{
    // Set simulation parameters
    double simulationTime = 30.0; // seconds
    uint32_t numNodes = 10;
    double txPower = 16.0; // dBm
    double nodeSpeed = 20.0; // m/s (random speed for mobility)

    // Create nodes
    NodeContainer adhocNodes;
    adhocNodes.Create(numNodes);

    // Set up Wi-Fi for ad-hoc network
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.Set("TxPowerStart", DoubleValue(txPower));
    phy.Set("TxPowerEnd", DoubleValue(txPower));

    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(100.0));
    phy.SetChannel(wifiChannel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices;
    devices = wifi.Install(phy, mac, adhocNodes);

    // Set up mobility
    MobilityHelper mobility;
    Ptr<UniformRandomVariable> speed = CreateObject<UniformRandomVariable>();
    speed->SetAttribute("Min", DoubleValue(0.0));
    speed->SetAttribute("Max", DoubleValue(nodeSpeed));

    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed", PointerValue(speed),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
                              "PositionAllocator", StringValue("ns3::RandomRectanglePositionAllocator"),
                              "X", DoubleValue(500.0), // Area of 500x500 meters
                              "Y", DoubleValue(500.0));

    mobility.Install(adhocNodes);

    // Install internet stack and AODV routing protocol
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv); // AODV as the routing protocol
    internet.Install(adhocNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up UDP traffic: a UdpEchoServer on one node and a UdpEchoClient on another
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(adhocNodes.Get(0)); // Server on node 0
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simulationTime));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(0), port); // Client to node 0
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(adhocNodes.Get(numNodes - 1)); // Client on last node
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(simulationTime));

    // Enable pcap tracing
    phy.EnablePcapAll("aodv-adhoc");

    // Install FlowMonitor for traffic statistics
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Output flow statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    uint32_t sentPackets = 0;
    uint32_t receivedPackets = 0;
    double delaySum = 0.0;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
        sentPackets += i->second.txPackets;
        receivedPackets += i->second.rxPackets;
        delaySum += i->second.delaySum.GetSeconds();
    }

    double pdr = double(receivedPackets) / sentPackets * 100;
    double averageDelay = delaySum / receivedPackets;

    NS_LOG_UNCOND("Packet Delivery Ratio: " << pdr << "%");
    NS_LOG_UNCOND("Average End-to-End Delay: " << averageDelay << " s");

    // Clean up
    Simulator::Destroy();
    return 0;
}

