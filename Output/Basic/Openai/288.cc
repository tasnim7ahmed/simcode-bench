#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ac);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-80211ac");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    MobilityHelper mobility;

    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                 "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));

    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
                              "PositionAllocator", PointerValue(mobility.GetPositionAllocator()));
    mobility.Install(wifiStaNodes);

    MobilityHelper apMobility;
    apMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    apMobility.Install(wifiApNode);

    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterfaces;
    apInterfaces = address.Assign(apDevice);

    uint16_t port = 5000;

    Address serverAddress(InetSocketAddress(staInterfaces.GetAddress(0), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory",
                                      InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApps = packetSinkHelper.Install(wifiStaNodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    BulkSendHelper clientHelper("ns3::TcpSocketFactory",
                                InetSocketAddress(staInterfaces.GetAddress(0), port));
    clientHelper.SetAttribute("MaxBytes", UintegerValue(0));
    clientHelper.SetAttribute("SendSize", UintegerValue(1024));

    ApplicationContainer clientApps = clientHelper.Install(wifiStaNodes.Get(1));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.EnablePcap("wifi-80211ac-ap", apDevice.Get(0), true);
    phy.EnablePcap("wifi-80211ac-sta1", staDevices.Get(0), true);
    phy.EnablePcap("wifi-80211ac-sta2", staDevices.Get(1), true);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}