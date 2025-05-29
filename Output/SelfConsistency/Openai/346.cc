#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging for echo applications (optional)
    // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer wifiNodes;
    wifiNodes.Create(2); // Node 0: AP, Node 1: STA

    // Wifi Channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Wifi Helper
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);

    // Wifi Mac Helpers
    WifiMacHelper mac;

    // SSID
    Ssid ssid = Ssid("ns3-80211n");

    // Configure STA device
    NetDeviceContainer staDevices;
    mac.SetType("ns3::StaWifiMac",
        "Ssid", SsidValue(ssid),
        "ActiveProbing", BooleanValue(false));
    staDevices = wifi.Install(phy, mac, wifiNodes.Get(1));

    // Configure AP device
    NetDeviceContainer apDevices;
    mac.SetType("ns3::ApWifiMac",
        "Ssid", SsidValue(ssid));
    apDevices = wifi.Install(phy, mac, wifiNodes.Get(0));

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
        "MinX", DoubleValue (0.0),
        "MinY", DoubleValue (0.0),
        "DeltaX", DoubleValue (2.0),
        "DeltaY", DoubleValue (0.0),
        "GridWidth", UintegerValue (2),
        "LayoutType", StringValue ("RowFirst")
    );
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    // Internet Stack
    InternetStackHelper stack;
    stack.Install(wifiNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    NetDeviceContainer allDevices;
    allDevices.Add(apDevices);
    allDevices.Add(staDevices);

    Ipv4InterfaceContainer interfaces = address.Assign(allDevices);

    // UDP Echo Server on AP (Node 0)
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(wifiNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Echo Client on STA (Node 1)
    UdpEchoClientHelper echoClient(interfaces.GetAddress(0), echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(9));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(wifiNodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable pcap tracing (optional)
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.EnablePcap("wifi-80211n-2nodes-ap", apDevices.Get(0));
    phy.EnablePcap("wifi-80211n-2nodes-sta", staDevices.Get(0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}