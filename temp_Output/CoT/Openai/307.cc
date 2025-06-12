#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Command line params if needed
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    // Wi-Fi channel and PHY setup
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Wi-Fi standard
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

    // MAC
    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

    // Set as AP for node 0, STA for node 1
    NetDeviceContainer devices;
    // Both as STA for general infra-less connectivity
    mac.SetType("ns3::AdhocWifiMac");
    devices = wifi.Install(phy, mac, wifiNodes);

    // Mobility models: fixed positions, node 0 at (0,0), node 1 at (5,0)
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
    posAlloc->Add(Vector(0.0, 0.0, 0.0));
    posAlloc->Add(Vector(5.0, 0.0, 0.0));
    mobility.SetPositionAllocator(posAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    // Stack, assign IP addresses
    InternetStackHelper stack;
    stack.Install(wifiNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP Echo Server on node 0, port 9
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(wifiNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Echo Client on node 1, targets node 0
    UdpEchoClientHelper echoClient(interfaces.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(wifiNodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable pcap tracing
    phy.EnablePcap("wifi-echo", devices);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}