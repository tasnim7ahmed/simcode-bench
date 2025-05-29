#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    NodeContainer staNodes;
    staNodes.Create(1);

    NodeContainer apNodes;
    apNodes.Create(1);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211a);

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
                    "Ssid", SsidValue(ssid),
                    "BeaconInterval", TimeValue(Seconds(0.1)));

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
    mobility.Install(apNodes);
    mobility.Install(staNodes);

    InternetStackHelper stack;
    stack.Install(apNodes);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    UdpClientHelper client(apInterfaces.GetAddress(0), 9);
    client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    client.SetAttribute("MaxPackets", UintegerValue(1000));

    ApplicationContainer clientApps = client.Install(staNodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    UdpServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(apNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}