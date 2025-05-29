#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    WifiMacHelper mac;

    Ssid ssid = Ssid("ns3-ssid");

    NetDeviceContainer devices;
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    devices.Add(wifi.Install(phy, mac, wifiNodes.Get(1)));

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    devices.Add(wifi.Install(phy, mac, wifiNodes.Get(0)));

    MobilityHelper mobility;

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // AP at (0,0,0)
    positionAlloc->Add(Vector(5.0, 0.0, 0.0)); // Initial STA position

    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes.Get(0)); // Stationary AP

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)),
                             "Distance", DoubleValue(2.0),
                             "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"));
    mobility.Install(wifiNodes.Get(1)); // Moving STA

    InternetStackHelper stack;
    stack.Install(wifiNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;

    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiNodes.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    UdpClientHelper client(interfaces.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(320));
    client.SetAttribute("Interval", TimeValue(Seconds(0.03125)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = client.Install(wifiNodes.Get(1));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}