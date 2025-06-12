#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/wifi-phy.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AodvManetSimulation");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 5;
    double duration = 10.0;
    double txp = 5.0; // m
    std::string phyMode("DsssRate1Mbps");
    std::string traceFile = "aodv_mobility_trace.tr";

    CommandLine cmd(__FILE__);
    cmd.AddValue("txp", "Transmission range (meters)", txp);
    cmd.AddValue("duration", "Simulation time, s", duration);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                  "DataMode", StringValue(phyMode),
                                  "ControlMode", StringValue(phyMode));

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobility.Install(nodes);

    AodvHelper aodv;

    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(numNodes - 1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(duration));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(numNodes - 1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numNodes - 1; ++i) {
        clientApps.Add(echoClient.Install(nodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(duration));

    AsciiTraceHelper ascii;
    MobilityHelper::EnableAsciiAll(ascii.CreateFileStream(traceFile));

    wifiPhy.EnablePcapAll("aodv_manet_simulation");

    Simulator::Stop(Seconds(duration));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}