#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiThreeNodeSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes: 0 (station), 1 (AP), 2 (station)
    NodeContainer nodes;
    nodes.Create(3);

    // Create Wi-Fi channel
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    NetDeviceContainer apDevice;
    NetDeviceContainer staDevices;

    // Setup AP
    wifiMac.SetType("ns3::ApWifiMac");
    apDevice = wifi.Install(wifiPhy, wifiMac, nodes.Get(1));

    // Setup STAs
    wifiMac.SetType("ns3::StaWifiMac");
    staDevices = wifi.Install(wifiPhy, wifiMac, nodes.Get(0));
    staDevices.Add(wifi.Install(wifiPhy, wifiMac, nodes.Get(2)));

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");

    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // Forwarding between AP and second station
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // UDP Echo Server on Node 2 (forwarded by AP)
    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Echo Client on Node 0 sending to Node 2 via AP
    UdpEchoClientHelper client(staInterfaces.GetAddress(2), port);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    client.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Simulation setup
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}