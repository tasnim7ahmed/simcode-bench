#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiTcpGridSimulation");

int main(int argc, char *argv[]) {
    uint32_t packetSize = 512;
    double interval = 0.1;
    uint32_t packetsPerClient = 10;
    double simulationTime = 20.0;

    NodeContainer nodes;
    nodes.Create(3);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    mac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;

    // TCP Server on node 2
    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp = sink.Install(nodes.Get(2));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simulationTime));

    // Client applications on node 0 and node 1
    OnOffHelper client0("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(2), port));
    client0.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    client0.SetAttribute("PacketSize", UintegerValue(packetSize));
    client0.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=0.1]"));
    client0.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    client0.SetAttribute("MaxBytes", UintegerValue(packetSize * packetsPerClient));

    ApplicationContainer clientApp0 = client0.Install(nodes.Get(0));
    clientApp0.Start(Seconds(1.0));
    clientApp0.Stop(Seconds(simulationTime));

    OnOffHelper client1("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(2), port));
    client1.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    client1.SetAttribute("PacketSize", UintegerValue(packetSize));
    client1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=0.1]"));
    client1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    client1.SetAttribute("MaxBytes", UintegerValue(packetSize * packetsPerClient));

    ApplicationContainer clientApp1 = client1.Install(nodes.Get(1));
    clientApp1.Start(Seconds(1.0));
    clientApp1.Stop(Seconds(simulationTime));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}