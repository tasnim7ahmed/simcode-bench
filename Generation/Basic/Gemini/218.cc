#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/dsdv-helper.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DsdvVanet");

int main(int argc, char *argv[])
{
    LogComponentEnable("DsdvRoutingProtocol", LOG_LEVEL_ALL);
    LogComponentEnable("DsdvHelper", LOG_LEVEL_ALL);
    LogComponentEnable("UdpClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("ConstantVelocityMobilityModel", LOG_LEVEL_INFO);
    LogComponentEnable("DsdvVanet", LOG_LEVEL_INFO);

    uint32_t nNodes = 20;
    double simulationTime = 60.0;
    double nodeSpeed = 20.0;
    double nodeSpacing = 10.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nNodes", "Number of nodes", nNodes);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("nodeSpeed", "Speed of nodes in m/s", nodeSpeed);
    cmd.AddValue("nodeSpacing", "Spacing between nodes in meters", nodeSpacing);
    cmd.Parse(argc, argv);

    if (nNodes < 2)
    {
        NS_FATAL_ERROR("Number of nodes must be at least 2 for communication.");
    }

    NodeContainer nodes;
    nodes.Create(nNodes);

    for (uint32_t i = 0; i < nNodes; ++i)
    {
        Ptr<ConstantVelocityMobilityModel> mob = CreateObject<ConstantVelocityMobilityModel>();
        mob->SetPosition(Vector(i * nodeSpacing, 0.0, 0.0));
        mob->SetVelocity(Vector(nodeSpeed, 0.0, 0.0));
        nodes.Get(i)->AggregateObject(mob);
    }

    YansWifiChannelHelper channel;
    channel.SetPropagationDelayModel(CreateObject<ConstantSpeedPropagationDelayModel>());
    channel.SetPropagationLossModel("ns3::LogDistancePropagationLossModel",
                                    "Exponent", DoubleValue(3.0),
                                    "ReferenceLoss", DoubleValue(40.0),
                                    "ReferenceDistance", DoubleValue(1.0));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);

    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    DsdvHelper dsdvRouting;
    dsdvRouting.Set("ActiveRouteTimeout", TimeValue(Seconds(30.0)));
    dsdvRouting.Set("HelloInterval", TimeValue(Seconds(1.0)));
    dsdvRouting.Set("NetDiameter", UintegerValue(7));

    InternetStackHelper internetStack;
    internetStack.SetRoutingHelper(dsdvRouting);
    internetStack.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    uint16_t port = 9;
    Ptr<Node> senderNode = nodes.Get(0);
    Ptr<Node> receiverNode = nodes.Get(nNodes - 1);
    Ipv4Address sinkAddress = interfaces.GetAddress(nNodes - 1);

    UdpClientHelper client(sinkAddress, port);
    client.SetAttribute("MaxPackets", UintegerValue(200));
    client.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(senderNode);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime - 1.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sink.Install(receiverNode);
    sinkApps.Start(Seconds(1.0));
    sinkApps.Stop(Seconds(simulationTime));

    phy.EnablePcap("vanet-dsdv", devices);

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.Install(nodes);

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckFor")";
    monitor->SerializeToXmlFile("vanet-dsdv.flowmon", true, true);

    Simulator::Destroy();

    NS_LOG_INFO("Simulation finished.");

    return 0;
}