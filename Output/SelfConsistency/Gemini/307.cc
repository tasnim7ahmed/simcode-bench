#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiEcho");

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(2);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, nodes.Get(0));

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(Seconds(1.0)));

    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, mac, nodes.Get(1));

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::FixedPositionAllocator");

    mobility.Install(nodes.Get(0));
    mobility.Install(nodes.Get(1));

    Ptr<MobilityModel> mob0 = nodes.Get(0)->GetObject<MobilityModel>();
    Ptr<MobilityModel> mob1 = nodes.Get(1)->GetObject<MobilityModel>();

    mob0->SetPosition(Vector(0.0, 0.0, 0.0));
    mob1->SetPosition(Vector(5.0, 0.0, 0.0));

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer::Create(staDevices, apDevices));

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}