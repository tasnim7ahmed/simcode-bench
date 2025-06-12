#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    uint32_t numNodes = 5;
    double simTime = 20.0;
    double distance = 100.0;

    NodeContainer nodes;
    nodes.Create(numNodes);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
        "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
        "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    uint16_t port = 4000;

    // UDP Server on node 0
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(nodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simTime));

    // UDP Client on node 4 sending to server (node 0)
    UdpClientHelper client(interfaces.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    client.SetAttribute("PacketSize", UintegerValue(64));

    ApplicationContainer clientApp = client.Install(nodes.Get(4));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(simTime - 1));

    // Enable packet tracing
    wifiPhy.EnablePcapAll("adhoc-aodv");

    // FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Enable routing table tracing
    Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>("aodv-routing.log", std::ios::out);
    aodv.PrintRoutingTableAllAt(Seconds(5.0), routingStream);
    aodv.PrintRoutingTableAllAt(Seconds(10.0), routingStream);
    aodv.PrintRoutingTableAllAt(Seconds(15.0), routingStream);

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    monitor->SerializeToXmlFile("adhoc-aodv-flowmon.xml", true, true);

    Simulator::Destroy();
    return 0;
}