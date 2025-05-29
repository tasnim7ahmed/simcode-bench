#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(3);

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("DsssRate11Mbps"),
                                 "ControlMode", StringValue("DsssRate11Mbps"));

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("ns3-80211b");
    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, nodes);

    // All nodes are STAs in this setup.
    // Set up Mobility: 3x3 grid layout but only 3 nodes used
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(20.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(staDevices);

    // TCP Server on node 2
    uint16_t port = 9;
    Address serverAddress(InetSocketAddress(interfaces.GetAddress(2), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory",
                                      InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp = packetSinkHelper.Install(nodes.Get(2));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(20.0));

    // TCP Clients on node 0 and node 1
    for (uint32_t i = 0; i < 2; ++i)
    {
        OnOffHelper clientHelper("ns3::TcpSocketFactory", serverAddress);
        clientHelper.SetAttribute("PacketSize", UintegerValue(512));
        clientHelper.SetAttribute("MaxBytes", UintegerValue(5120)); // 10 packets * 512 bytes
        clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        clientHelper.SetAttribute("DataRate", StringValue("51200bps")); // 512 bytes/0.1s = 4096 bytes/s = 32768 bps (use 51200 for margin)
        ApplicationContainer clientApp = clientHelper.Install(nodes.Get(i));
        clientApp.Start(Seconds(1.0 + i * 0.1)); // stagger start times a bit
        clientApp.Stop(Seconds(20.0));
    }

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}