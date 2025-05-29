#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    NodeContainer staNodes;
    staNodes.Create(1);

    NodeContainer apNodes;
    apNodes.Create(1);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("ns3-wifi");
    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, staNodes);

    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevices = wifi.Install(wifiPhy, wifiMac, apNodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(staNodes);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);

    InternetStackHelper stack;
    stack.Install(apNodes);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevices);
    Ipv4InterfaceContainer staInterface = address.Assign(staDevices);

    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(apNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpClientHelper client(apInterface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(100));
    client.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    client.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApp = client.Install(staNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}