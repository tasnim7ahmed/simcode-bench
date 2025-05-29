#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-ssid");

    // Install Wi-Fi devices on STAs
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Install Wi-Fi device on AP
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // Set mobility - fixed positions
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(5.0),
                                 "DeltaY", DoubleValue(10.0),
                                 "GridWidth", UintegerValue(3),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);

    // Assign IPs
    Ipv4AddressHelper address;
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    // UDP Echo server on STA1
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(wifiStaNodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    // UDP Echo client on STA0 targeting STA1
    UdpEchoClientHelper echoClient(staInterfaces.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = echoClient.Install(wifiStaNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(20.0));

    // Enable pcap tracing
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.EnablePcap("wifi-ap", apDevice.Get(0), true);
    phy.EnablePcap("wifi-sta0", staDevices.Get(0), true);
    phy.EnablePcap("wifi-sta1", staDevices.Get(1), true);

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}