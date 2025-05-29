#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/applications-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/csma-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    channel.SetDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss ("ns3::ConstantSpeedPropagationDelayModel");
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, mac, wifiNodes.Get(0));

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiNodes.Get(1));

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
        "MinX", DoubleValue (0.0),
        "MinY", DoubleValue (0.0),
        "DeltaX", DoubleValue (5.0),
        "DeltaY", DoubleValue (0.0),
        "GridWidth", UintegerValue (2),
        "LayoutType", StringValue ("RowFirst"));
    mobility.Install(wifiNodes);

    InternetStackHelper stack;
    stack.Install (wifiNodes);

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces;
    interfaces = address.Assign (NetDeviceContainer(staDevice, apDevice));

    // Fix Wi-Fi delay and rate
    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode",
                       StringValue("OfdmRate6Mbps"));

    // Install UDP Server on Node 1
    uint16_t port = 9;
    UdpServerHelper server (port);
    ApplicationContainer serverApps = server.Install (wifiNodes.Get(1));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (11.0));

    // Install UDP Client on Node 0
    uint32_t maxPackets = 1000;
    Time interPacketInterval = Seconds (0.01);
    UdpClientHelper client (interfaces.GetAddress (1), port);
    client.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
    client.SetAttribute ("Interval", TimeValue (interPacketInterval));
    client.SetAttribute ("PacketSize", UintegerValue (1024));
    ApplicationContainer clientApps = client.Install (wifiNodes.Get(0));
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (10.0));

    // Set constant rate and delay for the physical layer
    Config::SetDefault ("ns3::WifiPhy::DataRate", StringValue ("10Mbps"));
    Config::SetDefault ("ns3::PropagationDelayModel::Delay", TimeValue (MilliSeconds(2)));

    Simulator::Stop (Seconds (11.0));
    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}