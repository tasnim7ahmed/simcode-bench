#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiUdpRandomWalk");

int main(int argc, char* argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponent::SetFilter("UdpClient", LOG_LEVEL_INFO);
    LogComponent::SetFilter("UdpServer", LOG_LEVEL_INFO);

    NodeContainer staNodes;
    staNodes.Create(1);

    NodeContainer apNode;
    apNode.Create(1);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(Seconds(0.1)),
                "DtimPeriod", UintegerValue(1));

    NetDeviceContainer apDevices = wifi.Install(phy, mac, apNode);

    InternetStackHelper stack;
    stack.Install(staNodes);
    stack.Install(apNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staNodeIface = address.Assign(staDevices);
    Ipv4InterfaceContainer apNodeIface = address.Assign(apDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                  "Z", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                               "Mode", StringValue("Time"),
                               "Time", StringValue("1s"),
                               "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                               "Bounds", StringValue("100x100"));
    mobility.Install(staNodes);

    MobilityHelper apMobility;
    apMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    apMobility.Install(apNode);

    UdpServerHelper server(9);
    ApplicationContainer serverApps = server.Install(apNode);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpClientHelper client(apNodeIface.GetAddress(0), 9);
    client.SetAttribute("MaxPackets", UintegerValue(10));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(staNodes);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}