#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

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

    Ptr<UniformRandomVariable> random = CreateObject<UniformRandomVariable>();

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue(Rectangle(0.0, 100.0, 0.0, 100.0)),
                             "Distance", DoubleValue(10.0),
                             "Mode", StringValue("Time"),
                             "Time", TimeValue(Seconds(1.0))
    );
    mobility.Install(wifiStaNode);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);

    InternetStackHelper stack;
    stack.Install(wifiStaNode);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevice);
    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevice);

    // TCP Server on AP (port 9)
    uint16_t port = 9;
    Address apLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", apLocalAddress);
    ApplicationContainer sinkApps = packetSinkHelper.Install(wifiApNode.Get(0));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(20.0));

    // TCP Client on STA
    OnOffHelper clientHelper("ns3::TcpSocketFactory", InetSocketAddress(apInterfaces.GetAddress(0), port));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("8kbps"))); // Just enough for one packet per second
    clientHelper.SetAttribute("MaxBytes", UintegerValue(1024 * 10));
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer clientApps = clientHelper.Install(wifiStaNode.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(20.0));

    Simulator::Stop(Seconds(21.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}