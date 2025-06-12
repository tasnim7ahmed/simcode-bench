#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for UDP applications
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes: 0 is mobile, 1 is static
    NodeContainer nodes;
    nodes.Create(2);

    // Set up Wi-Fi in ad-hoc mode with 802.11s
    WifiMacHelper wifiMac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211s);

    wifiMac.SetType("ns3::AdhocWifiMac");

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup mobility for node 0 (mobile)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes.Get(0));
    Ptr<ConstantVelocityMobilityModel> mobileNodeModel =
        nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>();
    mobileNodeModel->SetPosition(Vector(0.0, 0.0, 0.0));
    mobileNodeModel->SetVelocity(Vector(1.0, 0.0, 0.0));  // Move along x-axis at 1 m/s

    // Setup static position for node 1 (static)
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes.Get(1));

    // Install UDP Echo Server on node 1
    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApp = server.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Install UDP Echo Client on node 0
    UdpEchoClientHelper client(interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(10));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable global routing to ensure connectivity in ad-hoc network
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}