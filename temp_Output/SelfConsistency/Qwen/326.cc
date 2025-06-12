#include "ns3/aodv-helper.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/ns3-udp-server.h"
#include "ns3/ns3-udp-client.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/simulator.h"
#include "ns3/wifi-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/yans-wifi-phy.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetAodvExample");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(5);

    // Setup mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(20.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed", StringValue("ns3::UniformRandomVariable[Min=0.5|Max=2.0]"),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
                              "PositionAllocator", PointerValue(CreateObject<RandomRectanglePositionAllocator>(
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"))));
    mobility.Install(nodes);

    // Setup Wi-Fi
    WifiMacHelper wifiMac;
    WifiPhyHelper wifiPhy;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(channel.Create());

    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifiMac.Install(wifiPhy, nodes);

    // Setup AODV routing
    aodv::AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(aodv);
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup applications
    uint16_t port = 9;
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApps = sink.Install(nodes.Get(4));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(4), port));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("100kbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = onoff.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable pcap tracing
    wifiPhy.EnablePcapAll("manet-aodv");

    // Set simulation stop time
    Simulator::Stop(Seconds(10.0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}