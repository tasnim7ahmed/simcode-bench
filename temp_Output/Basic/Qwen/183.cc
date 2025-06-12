#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTwoAPsFourSTAs");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Enable rts cts
    Config::SetDefault("ns3::WifiMac::RtsThreshold", UintegerValue(9000));

    NodeContainer apNodes;
    apNodes.Create(2);

    NodeContainer staNodes;
    staNodes.Create(4);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    WifiMacHelper mac;
    Ssid ssid1 = Ssid("wifi-default1");
    Ssid ssid2 = Ssid("wifi-default2");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid1),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices1 = wifi.Install(phy, mac, staNodes.Get(0));
    NetDeviceContainer staDevices2 = wifi.Install(phy, mac, staNodes.Get(1));
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid2),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices3 = wifi.Install(phy, mac, staNodes.Get(2));
    NetDeviceContainer staDevices4 = wifi.Install(phy, mac, staNodes.Get(3));

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid1));
    NetDeviceContainer apDevices1 = wifi.Install(phy, mac, apNodes.Get(0));
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid2));
    NetDeviceContainer apDevices2 = wifi.Install(phy, mac, apNodes.Get(1));

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(4),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);
    mobility.Install(staNodes);

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer apInterfaces1 = address.Assign(apDevices1);
    Ipv4InterfaceContainer apInterfaces2 = address.Assign(apDevices2);
    Ipv4InterfaceContainer staInterfaces1 = address.Assign(staDevices1);
    Ipv4InterfaceContainer staInterfaces2 = address.Assign(staDevices2);
    Ipv4InterfaceContainer staInterfaces3 = address.Assign(staDevices3);
    Ipv4InterfaceContainer staInterfaces4 = address.Assign(staDevices4);

    uint16_t port = 9;

    // UDP Echo Server on AP1
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(apNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Echo Client from STA1 to AP1
    UdpEchoClientHelper echoClient(apInterfaces1.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(staNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // UDP Echo Server on AP2
    serverApps = echoServer.Install(apNodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Echo Client from STA3 to AP2
    echoClient = UdpEchoClientHelper(apInterfaces2.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    clientApps = echoClient.Install(staNodes.Get(2));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    AsciiTraceHelper ascii;
    phy.EnableAsciiAll(ascii.CreateFileStream("wifi-two-aps-four-stas.tr"));
    phy.EnablePcapAll("wifi-two-aps-four-stas");

    AnimationInterface anim("wifi-two-aps-four-stas.xml");
    anim.SetConstantPosition(apNodes.Get(0), 0.0, 0.0);
    anim.SetConstantPosition(apNodes.Get(1), 10.0, 0.0);
    anim.SetConstantPosition(staNodes.Get(0), 2.0, 5.0);
    anim.SetConstantPosition(staNodes.Get(1), 4.0, 5.0);
    anim.SetConstantPosition(staNodes.Get(2), 6.0, 5.0);
    anim.SetConstantPosition(staNodes.Get(3), 8.0, 5.0);

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}