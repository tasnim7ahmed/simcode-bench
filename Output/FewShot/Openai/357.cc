#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;

    Ssid ssid = Ssid("ns3-wifi-ssid");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, mac, wifiStaNode);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    MobilityHelper mobility;

    Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
    posAlloc->Add(Vector(0.0, 0.0, 0.0));   // STA position
    posAlloc->Add(Vector(5.0, 0.0, 0.0));   // AP position
    mobility.SetPositionAllocator(posAlloc);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNode.Get(0));
    mobility.Install(wifiApNode.Get(0));

    InternetStackHelper stack;
    stack.Install(wifiStaNode);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterface;
    Ipv4InterfaceContainer apInterface;
    staInterface = address.Assign(staDevice);
    apInterface = address.Assign(apDevice);

    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(apInterface.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = echoClient.Install(wifiStaNode.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(9.0));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}