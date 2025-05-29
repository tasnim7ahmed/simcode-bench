#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);

    // Create Wifi nodes
    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    // Set up Wifi PHY and channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Set up Wifi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    WifiMacHelper mac;
    Ssid ssid = Ssid("wifi-network");

    // Station MAC (client)
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice = wifi.Install(phy, mac, wifiNodes.Get(0));

    // AP MAC (server)
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiNodes.Get(1));

    // Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiNodes);

    // IP addressing
    Ipv4AddressHelper address;
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces;
    interfaces.Add(address.Assign(staDevice));
    interfaces.Add(address.Assign(apDevice));

    // Install UDP server on AP
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(wifiNodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(15.0));

    // Install UDP client on STA
    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
    echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(wifiNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(15.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}