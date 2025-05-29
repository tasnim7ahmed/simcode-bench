#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer staNodes;
    staNodes.Create(1);
    NodeContainer apNode;
    apNode.Create(1);

    // Configure the wireless channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Configure the wifi MAC layer
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();

    // Configure the AP
    Ssid ssid = Ssid("ns-3-ssid");
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(MilliSeconds(100)));

    NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

    // Configure the STA
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer staDevice = wifi.Install(phy, mac, staNodes);

    // Mobility
    MobilityHelper mobility;

    mobility.SetPositionAllocator("ns3::FixedPositionAllocator");

    mobility.Install(apNode);
    mobility.Install(staNodes);

    Ptr<Node> ap = apNode.Get(0);
    Ptr<MobilityModel> apMobility = ap->GetObject<MobilityModel>();
    apMobility->SetPosition(Vector(10.0, 10.0, 0.0));

    Ptr<Node> sta = staNodes.Get(0);
    Ptr<MobilityModel> staMobility = sta->GetObject<MobilityModel>();
    staMobility->SetPosition(Vector(20.0, 20.0, 0.0));


    // Install the internet stack
    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterface = address.Assign(staDevice);

    // UDP Echo Server
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(apNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Echo Client
    UdpEchoClientHelper echoClient(apInterface.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(staNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(9.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}