#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-80211n");

    NetDeviceContainer staDevices;
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    NetDeviceContainer apDevice;
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    apDevice = wifi.Install(phy, mac, wifiApNode);

    MobilityHelper mobility;

    // STA mobility
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
        "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)),
        "Distance", DoubleValue(10.0),
        "Mode", EnumValue(RandomWalk2dMobilityModel::MODE_DISTANCE),
        "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
        "Direction", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=6.283185307]"));
    mobility.Install(wifiStaNodes);

    // AP mobility
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    Ptr<MobilityModel> apMobility = wifiApNode.Get(0)->GetObject<MobilityModel>();
    apMobility->SetPosition(Vector(50.0, 50.0, 0.0));

    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);

    // UDP Echo Server on AP (port 9)
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(wifiApNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(20.0));

    // UDP Echo Client on each STA
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
    {
        UdpEchoClientHelper echoClient(apInterface.GetAddress(0), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(100));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApps = echoClient.Install(wifiStaNodes.Get(i));
        clientApps.Start(Seconds(2.0 + i));
        clientApps.Stop(Seconds(20.0));
    }

    // Reverse: UDP Echo Server on each STA, client on AP
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
    {
        UdpEchoServerHelper reverseServer(9);
        ApplicationContainer reverseApps = reverseServer.Install(wifiStaNodes.Get(i));
        reverseApps.Start(Seconds(1.0));
        reverseApps.Stop(Seconds(20.0));

        UdpEchoClientHelper reverseClient(staInterfaces.GetAddress(i), 9);
        reverseClient.SetAttribute("MaxPackets", UintegerValue(100));
        reverseClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        reverseClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer apClientApp = reverseClient.Install(wifiApNode.Get(0));
        apClientApp.Start(Seconds(2.5 + i));
        apClientApp.Stop(Seconds(20.0));
    }

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}