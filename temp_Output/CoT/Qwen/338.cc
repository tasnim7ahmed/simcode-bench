#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiMobileDevicesSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes: two STAs and one AP
    NodeContainer staNodes;
    staNodes.Create(2);
    NodeContainer apNode;
    apNode.Create(1);

    // Create Wi-Fi channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // Set Wi-Fi standard to 802.11n (5GHz)
    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));

    // Configure MAC for AP
    mac.SetType("ns3::ApWifiMac");
    NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

    // Configure MAC for STAs
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("wifi-network")));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

    // Mobility models for STAs (Random Walk)
    MobilityHelper mobilitySta;
    mobilitySta.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                     "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                     "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobilitySta.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                 "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobilitySta.Install(staNodes);

    // Stationary mobility model for AP
    MobilityHelper mobilityAp;
    mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityAp.Install(apNode);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // Setup UDP echo server on AP at port 9
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(apNode.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // Setup UDP echo clients on STAs
    uint32_t packetSize = 1024;
    Time interPacketInterval = Seconds(1.0);
    UdpEchoClientHelper echoClient1(apInterface.GetAddress(0), 9);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient1.SetAttribute("Interval", TimeValue(interPacketInterval));
    echoClient1.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApp1 = echoClient1.Install(staNodes.Get(0));
    clientApp1.Start(Seconds(1.0));
    clientApp1.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient2(apInterface.GetAddress(0), 9);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient2.SetAttribute("Interval", TimeValue(interPacketInterval));
    echoClient2.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApp2 = echoClient2.Install(staNodes.Get(1));
    clientApp2.Start(Seconds(1.0));
    clientApp2.Stop(Seconds(10.0));

    // Enable PCAP tracing
    phy.EnablePcap("ap_device", apDevice.Get(0));
    phy.EnablePcap("sta_device0", staDevices.Get(0));
    phy.EnablePcap("sta_device1", staDevices.Get(1));

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}