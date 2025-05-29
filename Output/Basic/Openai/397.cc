#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    // Wi-Fi configuration (default 802.11n 2.4GHz)
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("ns3-wifi");

    // AP configuration
    NetDeviceContainer apDevices;
    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid));
    apDevices = wifi.Install(wifiPhy, wifiMac, wifiNodes.Get(0));

    // STA configuration
    NetDeviceContainer staDevices;
    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid));
    staDevices = wifi.Install(wifiPhy, wifiMac, wifiNodes.Get(1));

    NetDeviceContainer allDevices;
    allDevices.Add(apDevices);
    allDevices.Add(staDevices);

    // Static mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiNodes);

    // IP address assignment
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(allDevices);

    // UDP Server on STA (receiver)
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiNodes.Get(1));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(25.0));

    // UDP Client on AP (sender)
    UdpClientHelper client(interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(20)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = client.Install(wifiNodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(25.0));

    Simulator::Stop(Seconds(25.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}