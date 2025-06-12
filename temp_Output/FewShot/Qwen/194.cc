#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create two Access Points and multiple STAs
    NodeContainer apNodes;
    apNodes.Create(2);

    NodeContainer staNodes;
    staNodes.Create(4);  // Two stations per AP

    // Divide STAs into groups for each AP
    NodeContainer stasAp1 = NodeContainer(staNodes.Get(0), staNodes.Get(1));
    NodeContainer stasAp2 = NodeContainer(staNodes.Get(2), staNodes.Get(3));

    // Install Wi-Fi devices
    WifiHelper wifi = WifiHelper::Default();
    wifi.SetStandard(WIFI_STANDARD_80211n_5MHz);

    // Set up PHY and channel
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    WifiChannelHelper channel = WifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    // MAC layer configuration
    Ssid ssid1 = Ssid("wifi-network-1");
    Ssid ssid2 = Ssid("wifi-network-2");

    WifiMacHelper mac;
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid1),
                "BeaconInterval", TimeValue(Seconds(2.5)));

    NetDeviceContainer apDevices1 = wifi.Install(phy, mac, apNodes.Get(0));
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid1),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices1 = wifi.Install(phy, mac, stasAp1);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid2),
                "BeaconInterval", TimeValue(Seconds(2.5)));
    NetDeviceContainer apDevices2 = wifi.Install(phy, mac, apNodes.Get(1));

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid2),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices2 = wifi.Install(phy, mac, stasAp2);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(apNodes);
    stack.Install(staNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices1);
    Ipv4InterfaceContainer staInterfaces1 = address.Assign(staDevices1);

    address.SetBase("192.168.2.0", "255.255.255.0");
    apInterfaces.Add(address.Assign(apDevices2));
    Ipv4InterfaceContainer staInterfaces2 = address.Assign(staDevices2);

    // Setup server node (on second AP network)
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(apNodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Setup clients on all stations
    for (uint32_t i = 0; i < staInterfaces1.GetN(); ++i) {
        UdpEchoClientHelper echoClient(apInterfaces.GetAddress(1, 1), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(0)); // Continuous
        echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApp = echoClient.Install(stasAp1.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));
    }

    for (uint32_t i = 0; i < staInterfaces2.GetN(); ++i) {
        UdpEchoClientHelper echoClient(apInterfaces.GetAddress(1, 1), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(0)); // Continuous
        echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApp = echoClient.Install(stasAp2.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));
    }

    // Tracing
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("wifi-udp.tr");
    phy.EnableAsciiAll(stream);

    phy.EnablePcapAll("wifi-udp");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}